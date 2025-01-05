#include "connection.h"

#include <charconv>
#include <string_view>
#include <lsp/str.h>
#include <lsp/json/json.h>
#include <lsp/jsonrpc/jsonrpc.h>

#ifndef LSP_MESSAGE_DEBUG_LOG
	#ifdef NDEBUG
		#define LSP_MESSAGE_DEBUG_LOG 0
	#else
		#define LSP_MESSAGE_DEBUG_LOG 1
	#endif
#endif

#if LSP_MESSAGE_DEBUG_LOG
	#ifdef __APPLE__
		#include <os/log.h>
	#elif defined(_WIN32)
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>
	#endif
#endif

namespace lsp{
namespace{

/*
 * Message logging
 */

#if LSP_MESSAGE_DEBUG_LOG
void debugLogMessageJson([[maybe_unused]] const std::string& messageType, [[maybe_unused]] const lsp::json::Any& json)
{
#ifdef __APPLE__
	os_log_debug(OS_LOG_DEFAULT, "%{public}s", (messageType + ": " + lsp::json::stringify(json, true)).c_str());
#elif defined(_WIN32)
	OutputDebugStringA((messageType + ": " + lsp::json::stringify(json, true) + '\n').c_str());
#endif
}
#endif

} // namespace

/*
 * Connection
 */

Connection::Connection(std::istream& in, std::ostream& out)
	: m_in{in},
	  m_out{out}
{
}

void Connection::receiveNextMessage(RequestHandlerInterface& requestHandler, ResponseHandlerInterface& responseHandler)
{
	std::lock_guard lock{m_readMutex};

	if(m_in.peek() == std::char_traits<char>::eof())
		throw ConnectionError{"Connection lost"};

	const auto header = readMessageHeader();

	std::string content;
	content.resize(header.contentLength);
	m_in.read(&content[0], static_cast<std::streamsize>(header.contentLength));

	// Verify only after reading the entire message so no partially unread message is left in the stream

	std::string_view contentType{header.contentType};

	if(!contentType.starts_with("application/vscode-jsonrpc"))
		throw ProtocolError{"Unsupported or invalid content type: " + header.contentType};

	constexpr std::string_view charsetKey{"charset="};
	if(const auto idx = contentType.find(charsetKey); idx != std::string_view::npos)
	{
		auto charset = contentType.substr(idx + charsetKey.size());
		charset = str::trimView(charset.substr(0, charset.find(';')));

		if(charset != "utf-8" && charset != "utf8")
			throw ProtocolError{"Unsupported or invalid character encoding: " + std::string{charset}};
	}

	try
	{
		auto json = json::parse(content);
#if LSP_MESSAGE_DEBUG_LOG
		debugLogMessageJson("incoming", json);
#endif

		if(json.isObject())
		{
			auto message = jsonrpc::messageFromJson(std::move(json.object()));

			if(std::holds_alternative<jsonrpc::Request>(message))
				requestHandler.onRequest(std::move(std::get<jsonrpc::Request>(message)));
			else
				responseHandler.onResponse(std::move(std::get<jsonrpc::Response>(message)));
		}
		else if(json.isArray())
		{
			auto batch = jsonrpc::messageBatchFromJson(std::move(json.array()));

			if(std::holds_alternative<jsonrpc::RequestBatch>(batch))
				requestHandler.onRequestBatch(std::move(std::get<jsonrpc::RequestBatch>(batch)));
			else
				responseHandler.onResponseBatch(std::move(std::get<jsonrpc::ResponseBatch>(batch)));
		}
	}
	catch(const std::exception& e)
	{
		throw ConnectionError{e.what()};
	}
}

void Connection::sendRequest(jsonrpc::Request&& request)
{
	writeJsonMessage(jsonrpc::requestToJson(std::move(request)));
}

void Connection::sendResponse(jsonrpc::Response&& response)
{
	writeJsonMessage(jsonrpc::responseToJson(std::move(response)));
}

void Connection::sendRequestBatch(jsonrpc::RequestBatch&& batch)
{
	writeJsonMessage(jsonrpc::requestBatchToJson(std::move(batch)));
}

void Connection::sendResponseBatch(jsonrpc::ResponseBatch&& batch)
{
	writeJsonMessage(jsonrpc::responseBatchToJson(std::move(batch)));
}

Connection::MessageHeader Connection::readMessageHeader()
{
	MessageHeader header;

	while(m_in.peek() != '\r')
		readNextMessageHeaderField(header);

	m_in.get(); // \r

	if(m_in.peek() != '\n')
		throw ProtocolError{"Invalid message header format"};

	m_in.get(); // \n

	return header;
}

void Connection::readNextMessageHeaderField(MessageHeader& header)
{
	if(m_in.peek() == std::char_traits<char>::eof())
		throw ConnectionError{"Connection lost"};

	std::string lineData;
	std::getline(m_in, lineData); // This also consumes the newline so it's only necessary to check for one \r\n before the content

	std::string_view line{lineData};
	const auto separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos)
	{
		const auto key   = str::trimView(line.substr(0, separatorIdx));
		const auto value = str::trimView(line.substr(separatorIdx + 1));

		if(key == "Content-Length")
			std::from_chars(value.data(), value.data() + value.size(), header.contentLength);
		else if(key == "Content-Type")
			header.contentType = std::string{value.data(), value.size()};
	}
}

void Connection::writeJsonMessage(const json::Any& content)
{
#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("outgoing", content);
#endif
	writeMessage(json::stringify(content));
}

void Connection::writeMessage(const std::string& content)
{
	std::lock_guard lock{m_writeMutex};
	MessageHeader header{content.size()};
	writeMessageHeader(header);
	m_out.write(content.data(), static_cast<std::streamsize>(content.size()));
	m_out.flush();
}

void Connection::writeMessageHeader(const MessageHeader& header)
{
	const auto headerStr = "Content-Length: " + std::to_string(header.contentLength) + "\r\n\r\n";
	m_out.write(headerStr.data(), static_cast<std::streamsize>(headerStr.length()));
}

} // namespace lsp
