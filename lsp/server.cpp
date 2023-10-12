#include "server.h"

#include <string>
#include <cassert>
#include <charconv>
#include <string_view>

#include <json/json.h>
#include <jsonrpc/jsonrpc.h>

namespace stringutil{
	std::string_view trimLeft(std::string_view str){
		str.remove_prefix(std::distance(str.begin(), std::find_if(str.begin(), str.end(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	std::string_view trimRight(std::string_view str){
		str.remove_suffix(std::distance(str.rbegin(), std::find_if(str.rbegin(), str.rend(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	std::string_view trim(std::string_view str){
		str = trimLeft(str);
		return trimRight(str);
	}

	std::string_view trim(std::string_view&& str) = delete;
	std::string_view trimLeft(std::string_view&& str) = delete;
	std::string_view trimRight(std::string_view&& str) = delete;
}

namespace lsp{

/*
 * Connectoin
 */

Connection::Connection(std::istream& in, std::ostream& out) : m_in{in},
                                                              m_out{out}{}

void Connection::writeMessage(const jsonrpc::Response& response){
	std::string content = jsonrpc::responseToJsonString(response);
	assert(!content.empty());
	std::string header = "Content-Length: " + std::to_string(content.size()) + "\r\n"
	                     "Content-Type: utf-8\r\n\r\n";

	m_out.write(header.data(), static_cast<std::streamsize>(header.size()));
	m_out.write(content.data(), static_cast<std::streamsize>(content.size()));
	m_out.flush();
}

jsonrpc::Request Connection::readNextMessage(){
	auto header = readMessageHeader();
	// TODO: Error check
	m_in.get(); // \r
	m_in.get(); // \n

	std::string message;
	message.resize(header.contentLength);
	m_in.read(&message[0], static_cast<std::streamsize>(header.contentLength));

	if(header.contentType != "utf-8" && header.contentType != "utf8"){
		// ERROR: Unsupported content type
	}

	return jsonrpc::parseRequest(message);
}

bool Connection::readMessageHeaderField(MessageHeader& header){
	if(m_in.peek() == '\r')
		return false;

	std::string lineData;
	std::getline(m_in, lineData); // This also consumes the newline so it's only necessary to check for one \r\n before the data

	std::string_view line{lineData};
	std::size_t separatorIdx = line.find(':');

	if(separatorIdx != std::string_view::npos){
		std::string_view key = line.substr(0, separatorIdx);
		std::string_view value = line.substr(separatorIdx + 1);
		stringutil::trim(key);
		stringutil::trim(value);

		if(key == "Content-Length")
			std::from_chars(value.data(), value.data() + value.size(), header.contentLength);
		else if(key == "Content-Type")
			header.contentType = {value.data(), value.size()};
	}

	return false;
}

Connection::MessageHeader Connection::readMessageHeader(){
	MessageHeader header;

	while(readMessageHeaderField(header)){}

	return header;
}

/*
 * Server
 */

Server::Server(std::istream& in, std::ostream& out) : m_connection{in, out}{}

int Server::run(){
	try{
		auto message = m_connection.readNextMessage();
	}catch(const json::ParseError& e){

	}catch(...){
		return -1;
	}

	return EXIT_SUCCESS;
}

}
