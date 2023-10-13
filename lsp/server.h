#pragma once

#include <istream>
#include <ostream>
#include "jsonrpc.h"

namespace lsp{

class Connection{
public:
	Connection(std::istream& in, std::ostream& out);

	void writeMessage(const jsonrpc::Response& response);
	jsonrpc::Request readNextMessage();

private:
	std::istream& m_in;
	std::ostream& m_out;

	struct MessageHeader{
		std::size_t contentLength = 0;
		std::string contentType = "application/vscode-jsonrpc; charset=utf-8"; // Not always given, so just assume utf-8 by default. The protocol does not support anything else anyway.
	};

	MessageHeader readMessageHeader();
	bool readMessageHeaderField(MessageHeader& header);
	void writeMessageHeader(const MessageHeader& header);
};

class Server{
public:
	Server(std::istream& in, std::ostream& out);
	int run();

private:
	Connection m_connection;
};

}
