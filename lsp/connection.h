#pragma once

#include <mutex>
#include <string>
#include <istream>
#include <ostream>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

/*
 * Connection between the server and a client.
 * I/O happens via std::istream and std::ostream so the underlying implementation can be anything frmo stdio to sockets
 */
class Connection{
public:
	Connection(std::istream& in, std::ostream& out);

	std::variant<jsonrpc::MessagePtr, jsonrpc::MessageBatch> receiveMessage();
	void sendMessage(const jsonrpc::Message& message);
	void sendMessageBatch(const jsonrpc::MessageBatch& batch);

private:
	std::istream& m_in;
	std::ostream& m_out;
	std::mutex    m_readMutex;
	std::mutex    m_writeMutex;

	struct MessageHeader{
		std::size_t contentLength = 0;
		// Not always given, so just assume utf-8 by default. The protocol does not support anything else anyway
		std::string contentType = "application/vscode-jsonrpc; charset=utf-8";
	};

	MessageHeader readMessageHeader();
	void readNextMessageHeaderField(MessageHeader& header);
	void writeJsonMessage(const json::Any& content);
	void writeMessage(const std::string& content);
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

}
