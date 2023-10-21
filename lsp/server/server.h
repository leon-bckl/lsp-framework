#pragma once

#include <istream>
#include <ostream>
#include <stdexcept>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp::server{

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
	void readNextMessageHeaderField(MessageHeader& header);
	void writeMessageHeader(const MessageHeader& header);
};

class ProtocolError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};


class ConnectionError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

class LanguageAdapter;

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter);

}
