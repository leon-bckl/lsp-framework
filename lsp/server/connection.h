#pragma once

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

	jsonrpc::MessagePtr readNextMessage();
	void writeMessage(const jsonrpc::Message& message);

private:
	std::istream& m_in;
	std::ostream& m_out;

	struct MessageHeader{
		std::size_t contentLength = 0;
		// Not always given, so just assume utf-8 by default. The protocol does not support anything else anyway
		std::string contentType = "application/vscode-jsonrpc; charset=utf-8";
	};

	MessageHeader readMessageHeader();
	void readNextMessageHeaderField(MessageHeader& header);
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
