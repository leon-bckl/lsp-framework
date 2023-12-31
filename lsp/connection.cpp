#include "connection.h"

#include <cassert>
#include <charconv>
#include <string_view>
#include <lsp/util/str.h>

#ifndef ENABLE_DEBUG_MESSAGE_LOG
	#ifdef NDEBUG
		#define ENABLE_DEBUG_MESSAGE_LOG 0
	#else
		#define ENABLE_DEBUG_MESSAGE_LOG 1
	#endif
#endif

#if ENABLE_DEBUG_MESSAGE_LOG
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

#if ENABLE_DEBUG_MESSAGE_LOG
void debugLogMessageJson([[maybe_unused]] const std::string& messageType, [[maybe_unused]] const lsp::json::Any& json)
{
#ifdef __APPLE__
	os_log_debug(OS_LOG_DEFAULT, "%{public}s", (messageType + ": " + lsp::json::stringify(json, true)).c_str());
#elif defined(_WIN32)
	OutputDebugStringA((messageType + ": " + lsp::json::stringify(json, true) + '\n').c_str());
#endif
}
#endif

}

/*
 * Connection
 */

Connection::Connection(std::istream& in, std::ostream& out) : m_in{in},
                                                              m_out{out}{}

std::variant<jsonrpc::MessagePtr, std::vector<jsonrpc::MessagePtr>> Connection::receiveMessage()
{
	std::lock_guard lock{m_readMutex};

	if(m_in.peek() == std::char_traits<char>::eof())
		throw ConnectionError{"Connection lost"};

	auto header = readMessageHeader();

	std::string content;
	content.resize(header.contentLength);
	m_in.read(&content[0], static_cast<std::streamsize>(header.contentLength));

	// Verify only after reading the entire message so no partial unread message is left in the stream

	std::string_view contentType{header.contentType};

	if(!contentType.starts_with("application/vscode-jsonrpc"))
		throw ProtocolError{"Unsupported or invalid content type: " + header.contentType};

	const std::string_view charsetKey{"charset="};
	if(auto idx = contentType.find(charsetKey); idx != std::string_view::npos)
	{
		auto charset = contentType.substr(idx + charsetKey.size());
		charset = util::str::trimView(charset.substr(0, charset.find(';')));

		if(charset != "utf-8" && charset != "utf8")
			throw ProtocolError{"Unsupported or invalid character encoding: " + std::string{charset}};
	}

	auto json = json::parse(content);
#if ENABLE_DEBUG_MESSAGE_LOG
	debugLogMessageJson("incoming", json);
#endif
	return jsonrpc::messageFromJson(json);
}

void Connection::sendMessage(const jsonrpc::Message& message)
{
	writeJsonMessage(message.toJson());
}

void Connection::sendMessageBatch(const jsonrpc::MessageBatch& batch)
{
	json::Any content = json::Array{};
	auto& array = content.get<json::Array>();
	array.reserve(batch.size());

	for(const auto& m : batch)
		array.push_back(m->toJson());

	writeJsonMessage(content);
}

void Connection::writeJsonMessage(const json::Any& content)
{
#if ENABLE_DEBUG_MESSAGE_LOG
	debugLogMessageJson("outgoing", content);
#endif
	std::string contentStr = json::stringify(content);
	assert(!contentStr.empty());
	MessageHeader header{contentStr.size()};
	writeMessage(contentStr);
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
	assert(header.contentLength > 0);
	assert(!header.contentType.empty());
	std::string headerStr = "Content-Length: " + std::to_string(header.contentLength) + "\r\n\r\n";
	m_out.write(headerStr.data(), static_cast<std::streamsize>(headerStr.length()));
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
	std::size_t separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos)
	{
		std::string_view key = util::str::trimView(line.substr(0, separatorIdx));
		std::string_view value = util::str::trimView(line.substr(separatorIdx + 1));

		if(key == "Content-Length")
			std::from_chars(value.data(), value.data() + value.size(), header.contentLength);
		else if(key == "Content-Type")
			header.contentType = std::string{value.data(), value.size()};
	}
}

} // namespace lsp
