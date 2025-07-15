#pragma once

#include <mutex>
#include <string>
#include <lsp/exception.h>

namespace lsp{
namespace json{
class Any;
} // namespace json

namespace io{
class Stream;
} // namespace io

/*
 * Connection between the server and a client.
 * I/O happens via lsp::io::Stream so the underlying implementation can be anything from stdio to sockets
 */
class Connection{
public:
	Connection(io::Stream& stream);

	json::Any readMessage();
	void writeMessage(const json::Any& content);

private:
	io::Stream& m_stream;
	std::mutex  m_readMutex;
	std::mutex  m_writeMutex;

	struct MessageHeader;
	class InputReader;

	MessageHeader readMessageHeader(InputReader& reader);
	static void parseHeaderValue(MessageHeader& header, std::string_view line);
	static void readNextMessageHeaderField(MessageHeader& header, InputReader& reader);
	void writeMessageData(const std::string& content);
	std::string messageHeaderString(const MessageHeader& header);
};

/*
 * Error thrown when then connection to a client is unexpectedly lost
 */
class ConnectionError : public Exception{
public:
	using Exception::Exception;
};

} // namespace lsp
