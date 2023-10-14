#include "server.h"

#include <string>
#include <cassert>
#include <charconv>
#include <string_view>
#include <lsp/util/util.h>

namespace lsp::server{

/*
 * Connectoin
 */

Connection::Connection(std::istream& in, std::ostream& out) : m_in{in},
                                                              m_out{out}{}

void Connection::writeMessage(const jsonrpc::Response& response){
	std::string content = jsonrpc::responseToJsonString(response);
	assert(!content.empty());
	MessageHeader header{content.size()};
	writeMessageHeader(header);
	m_out.write(content.data(), static_cast<std::streamsize>(content.size()));
	m_out.flush();
}

void Connection::writeMessageHeader(const MessageHeader& header){
	assert(header.contentLength > 0);
	assert(!header.contentType.empty());
	std::string headerStr = "Content-Length: " + std::to_string(header.contentLength) + "\r\n"
	                        "Content-Type: utf-8\r\n\r\n";
	m_out.write(headerStr.data(), static_cast<std::streamsize>(headerStr.length()));
}

jsonrpc::Request Connection::readNextMessage(){
	auto header = readMessageHeader();
	// TODO: Error check
	m_in.get(); // \r
	m_in.get(); // \n

	std::string content;
	content.resize(header.contentLength);
	m_in.read(&content[0], static_cast<std::streamsize>(header.contentLength));

	// Verify only after reading the entire message so no partial unread message is left in the stream

	std::string_view contentType{header.contentType};

	if(!contentType.starts_with("application/vscode-jsonrpc")){
		// ERROR: Unsupported content type
	}

	const std::string_view charsetKey{"charset="};
	if(auto idx = contentType.find(charsetKey); idx != std::string_view::npos){
		auto charset = contentType.substr(idx + charsetKey.size());
		charset = util::str::trimView(charset.substr(0, charset.find(';')));

		if(charset != "utf-8" && charset != "utf8"){
			// ERROR: Unsupported charset
		}
	}

	return jsonrpc::parseRequest(content);
}

Connection::MessageHeader Connection::readMessageHeader(){
	MessageHeader header;

	while(readNextMessageHeaderField(header)){}

	return header;
}

bool Connection::readNextMessageHeaderField(MessageHeader& header){
	if(m_in.peek() == '\r')
		return false;

	std::string lineData;
	std::getline(m_in, lineData); // This also consumes the newline so it's only necessary to check for one \r\n before the content

	std::string_view line{lineData};
	std::size_t separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos){
		std::string_view key = util::str::trimView(line.substr(0, separatorIdx));
		std::string_view value = util::str::trimView(line.substr(separatorIdx + 1));

		if(key == "Content-Length")
			std::from_chars(value.data(), value.data() + value.size(), header.contentLength);
		else if(key == "Content-Type")
			header.contentType = {value.data(), value.size()};
	}

	return false;
}

/*
 * Server
 */

int start(std::istream& in, std::ostream& out){
	Connection connection{in, out};

	try{
		auto message = connection.readNextMessage();
	}catch(const json::ParseError& e){

	}catch(...){
		return -1;
	}

	return EXIT_SUCCESS;
}

}
