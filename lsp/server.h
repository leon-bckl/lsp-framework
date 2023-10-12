#pragma once

#include <istream>
#include <ostream>
#include <jsonrpc/jsonrpc.h>

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
		std::string contentType = "utf-8"; // Not always given, so just assume utf-8 by default. The protocol does not support anything else anyway.
	};

	bool readMessageHeaderField(MessageHeader& header);
	MessageHeader readMessageHeader();
};

class Server{
public:
	Server(std::istream& in, std::ostream& out);
	int run();

private:
	Connection m_connection;
};

}
