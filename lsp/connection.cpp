#include <algorithm>
#include <charconv>
#include <cstring>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <lsp/connection.h>
#include <lsp/error.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>

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
	#else
		#include <cstdio>
	#endif
#endif

namespace lsp{
namespace{

/*
 * Message logging
 */

#if LSP_MESSAGE_DEBUG_LOG
void debugLogMessageJson([[maybe_unused]] const std::string& messageType, [[maybe_unused]] const std::string& json)
{
#ifdef __APPLE__
	os_log_debug(OS_LOG_DEFAULT, "%{public}s", (messageType + ": " + json).c_str());
#elif defined(_WIN32)
	OutputDebugStringA((messageType + ": " + json + '\n').c_str());
#elif defined(__linux__) || defined(__HAIKU__)
	std::fprintf(stderr, "%s\n",  (messageType + ": " + json).c_str());
#endif
}
#endif

std::string_view trimWhitespace(std::string_view str)
{
	while(!str.empty() && str.front() <= 0x20)
		str.remove_prefix(1);

	while(!str.empty() && str.back() <= 0x20)
		str.remove_suffix(1);

	return str;
}

bool equalCaseInsensitive(std::string_view lhs, std::string_view rhs)
{
	return std::ranges::equal(lhs, rhs, [](char a, char b)
		{
			const auto toLower = [](char c){ return c >= 'A' && c <= 'Z' ? c + 32 : c; };
			return toLower(a) == toLower(b);
		});
}

void verifyContentType(std::string_view contentType)
{
	if(!contentType.starts_with("application/vscode-jsonrpc"))
		throw ConnectionError("Protocol: Unsupported or invalid content type: " + std::string(contentType));

	constexpr auto charsetKey = std::string_view("charset=");
	if(const auto idx = contentType.find(charsetKey); idx != std::string_view::npos)
	{
		auto charset = contentType.substr(idx + charsetKey.size());
		charset = trimWhitespace(charset.substr(0, charset.find(';')));

		if(charset != "utf-8" && charset != "utf8")
			throw ConnectionError("Protocol: Unsupported or invalid character encoding: " + std::string{charset});
	}
}

} // namespace

/*
 * Connection::InputReader
 * Wrapper around io::Stream that allows for peeking and reading single chars
 */

class Connection::InputReader{
public:
	InputReader(io::Stream& stream)
		: m_stream{stream}
	{
	}

	char peek()
	{
		if(!m_peek.has_value())
			m_peek = get();

		return m_peek.value();
	}

	char get()
	{
		if(m_peek.has_value())
		{
			const char c = m_peek.value();
			m_peek.reset();
			return c;
		}

		char c = io::Stream::Eof;
		read(&c, 1);
		return c;
	}

	void read(char* buffer, std::size_t size)
	{
		if(size > 0)
		{
			if(m_peek.has_value())
			{
				*buffer = m_peek.value();
				m_peek.reset();
				++buffer;
				--size;
			}

			m_stream.read(buffer, size);
		}
	}

private:
	io::Stream&         m_stream;
	std::optional<char> m_peek;
};

/*
 * Connection
 */

struct Connection::Internal{
	io::Stream& stream;
	std::mutex  readMutex;
	std::mutex  writeMutex;

	Internal(io::Stream& _stream) : stream{_stream}{}
};

struct Connection::MessageHeader{
	std::size_t contentLength = 0;
	std::string contentType   = "application/vscode-jsonrpc; charset=utf-8";
};

Connection::Connection(io::Stream& stream)
	: m{std::make_unique<Internal>(stream)}
{
}

Connection::~Connection()                                = default;
Connection::Connection(Connection&&) noexcept            = default;
Connection& Connection::operator=(Connection&&) noexcept = default;

Connection::Message Connection::readMessage()
{
	try
	{
		auto readLock = std::unique_lock(m->readMutex);
		auto reader   = InputReader(m->stream);

		if(reader.peek() == io::Stream::Eof)
			throw ConnectionError("Connection lost");

		const auto header = readMessageHeader(reader);

		std::string content;
		content.resize(header.contentLength);
		reader.read(&content[0], header.contentLength);

		readLock.unlock();

		// Verify only after reading the entire message so no partially unread message is left in the stream
		verifyContentType(header.contentType);

		auto json = json::parse(content);
#if LSP_MESSAGE_DEBUG_LOG
		debugLogMessageJson("incoming", json::stringify(json));
#endif

		if(json.isObject())
			return jsonrpc::messageFromJson(std::move(json.object()));

		if(!json.isArray())
			throw jsonrpc::ProtocolError("Message must be a json object or array");

		return jsonrpc::messageBatchFromJson(std::move(json.array()));
	}
	catch(const json::ParseError& e)
	{
		auto responseSender = errorResponse(json::Null(), MessageError::ParseError, e.what());
		responseSender.submit(*this);
		throw; // FIXME: This shouldn't abort the connection
	}
	catch(const jsonrpc::ProtocolError& e)
	{
		auto responseSender = errorResponse(json::Null(), MessageError::InvalidRequest, e.what());
		responseSender.submit(*this);
		throw; // FIXME: This shouldn't abort the connection
	}
	catch(const ConnectionError&)
	{
		throw;
	}
	catch(const std::exception& e)
	{
		throw ConnectionError(e.what());
	}
	catch(...)
	{
		throw ConnectionError("Unknown error");
	}
}

Connection::MessageHeader Connection::readMessageHeader(InputReader& reader)
{
	MessageHeader header;

	while(reader.peek() != '\r')
		readNextMessageHeaderField(header, reader);

	if(reader.get() != '\r' || reader.get() != '\n')
		throw ConnectionError("Protocol: Expected header to be terminated by '\\r\\n'");

	return header;
}

void Connection::parseHeaderValue(MessageHeader& header, std::string_view line)
{
	const auto separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos)
	{
		const auto key   = trimWhitespace(line.substr(0, separatorIdx));
		const auto value = trimWhitespace(line.substr(separatorIdx + 1));

		if(equalCaseInsensitive(key, "Content-Length"))
		{
			const auto* first    = value.data();
			const auto* last     = first + value.size();
			const auto [ptr, ec] = std::from_chars(first, last, header.contentLength);

			if(ec != std::errc{} || ptr != last)
				throw ConnectionError("Protocol: Invalid value for Content-Length header field");
		}
		else if(equalCaseInsensitive(key, "Content-Type"))
		{
			header.contentType = std::string{value.data(), value.size()};
		}
	}
}

void Connection::readNextMessageHeaderField(MessageHeader& header, InputReader& reader)
{
	if(reader.peek() == std::char_traits<char>::eof())
		throw ConnectionError{"Connection lost"};

	std::string lineData;

	while(reader.peek() != '\r')
	{
		const auto c = reader.get();

		if(c == '\n')
			throw ConnectionError("Protocol: Unexpected '\\n' in header field, expected '\\r\\n'");

		lineData.push_back(c);
	}

	parseHeaderValue(header, lineData);

	if(reader.get() != '\r' || reader.get() != '\n')
		throw ConnectionError("Protocol: Expected header field to be terminated by '\\r\\n'");
}

void Connection::writeMessageData(std::string_view content)
{
	const auto lock      = std::lock_guard(m->writeMutex);
	const auto header    = MessageHeader{ .contentLength = content.size() };
	const auto headerStr = messageHeaderString(header);

	try
	{
		m->stream.write(headerStr.data(), headerStr.size());
		m->stream.write(content.data(), content.size());
	}
	catch(const std::exception& e)
	{
		throw ConnectionError(e.what());
	}
}

std::string Connection::messageHeaderString(const MessageHeader& header)
{
	return "Content-Length: " + std::to_string(header.contentLength) + "\r\n" +
	       "Content-Type: " + header.contentType + "\r\n\r\n";
}

Connection::RequestSender Connection::request(std::string_view method, const jsonrpc::MessageId& id)
{
	return RequestSender(method, id);
}

Connection::RequestSender Connection::notification(std::string_view method)
{
	return RequestSender(method);
}

Connection::ResponseSender Connection::response(const jsonrpc::MessageId& id)
{
	return ResponseSender(id);
}

Connection::ResponseSender Connection::errorResponse(const jsonrpc::MessageId& id, int code, std::string_view message)
{
	return ResponseSender(id, code, message);
}

Connection::BatchSender Connection::messageBatch()
{
	return BatchSender();
}

/*
 * Connection::MessageSender
 */

Connection::MessageSender::MessageSender()
	: m_writer{m_buffer
#if LSP_MESSAGE_DEBUG_LOG
		, "\t"
#endif
	}
{
}

std::string_view Connection::MessageSender::buffer()
{
	return m_buffer;
}

json::Writer& Connection::MessageSender::writer()
{
	return m_writer;
}

/*
 * Connection::RequestSender
 */

Connection::RequestSender::RequestSender(std::string_view method, const jsonrpc::MessageId& id)
	: jsonrpc::RequestWriter{jsonrpc::RequestWriter::writeRequest(writer().beginObject(), id, method)}
{
}

Connection::RequestSender::RequestSender(std::string_view method)
	: jsonrpc::RequestWriter{jsonrpc::RequestWriter::writeNotification(writer().beginObject(), method)}
{
}

void Connection::RequestSender::submit(Connection& connection)
{
	finalize();
#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("outgoing", std::string(buffer()));
#endif
	connection.writeMessageData(buffer());
}

/*
 * Connection::ResponseSender
 */

Connection::ResponseSender::ResponseSender(const jsonrpc::MessageId& id)
	: jsonrpc::ResponseWriter{jsonrpc::ResponseWriter::writeResponse(writer().beginObject(), id)}
{
}

Connection::ResponseSender::ResponseSender(const jsonrpc::MessageId& id, int code, std::string_view message)
	: jsonrpc::ResponseWriter{jsonrpc::ResponseWriter::writeError(writer().beginObject(), id, code, message)}
{
}

void Connection::ResponseSender::submit(Connection& connection)
{
	finalize();
#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("outgoing", std::string(buffer()));
#endif
	connection.writeMessageData(buffer());
}

/*
 * Connection::BatchSender
 */

Connection::BatchSender::BatchSender()
	: jsonrpc::BatchWriter{writer().beginArray()}
{
}

void Connection::BatchSender::submit(Connection& connection)
{
	finalize();
#if LSP_MESSAGE_DEBUG_LOG
	debugLogMessageJson("outgoing", std::string(buffer()));
#endif
	connection.writeMessageData(buffer());
}

} // namespace lsp
