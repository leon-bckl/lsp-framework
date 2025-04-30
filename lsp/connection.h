#pragma once

#include <mutex>
#include <string>
#include <istream>
#include <ostream>

namespace lsp{
namespace json{
class Any;
}

/*
 * Connection between the server and a client.
 * I/O happens via std::istream and std::ostream so the underlying implementation can be anything from stdio to sockets
 */
class Connection{
public:
	Connection(std::istream& in, std::ostream& out);

	json::Any readMessage();
	void writeMessage(const json::Any& content);

private:
	std::istream& m_in;
	std::ostream& m_out;
	std::mutex    m_readMutex;
	std::mutex    m_writeMutex;

	struct MessageHeader;

	MessageHeader readMessageHeader();
	void readNextMessageHeaderField(MessageHeader& header);
	void writeMessageData(const std::string& content);
	void writeMessageHeader(const MessageHeader& header);
};

/*
 * Error thrown when then connection to a client is unexpectedly lost
 */
class ConnectionError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

/*
 * Error thrown when there was a problem parsing the incoming message
 */
class ProtocolError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

} // namespace lsp
