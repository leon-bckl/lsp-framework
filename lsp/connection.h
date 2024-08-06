#pragma once

#include <mutex>
#include <string>
#include <istream>
#include <ostream>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

class RequestHandlerInterface{
public:
	virtual void onRequest(jsonrpc::Request&& request) = 0;
	virtual void onRequestBatch(jsonrpc::RequestBatch&& batch) = 0;
};

class ResponseHandlerInterface{
public:
	virtual void onResponse(jsonrpc::Response&& response) = 0;
	virtual void onResponseBatch(jsonrpc::ResponseBatch&& response) = 0;
};

/*
 * Connection between the server and a client.
 * I/O happens via std::istream and std::ostream so the underlying implementation can be anything from stdio to sockets
 */
class Connection{
public:
	Connection(std::istream& in, std::ostream& out);

	void receiveNextMessage(RequestHandlerInterface& requestHandler, ResponseHandlerInterface& responseHandler);
	void sendRequest(jsonrpc::Request&& request);
	void sendResponse(jsonrpc::Response&& response);
	void sendRequestBatch(jsonrpc::RequestBatch&& batch);
	void sendResponseBatch(jsonrpc::ResponseBatch&& batch);

private:
	std::istream& m_in;
	std::ostream& m_out;
	std::mutex    m_readMutex;
	std::mutex    m_writeMutex;

	struct MessageHeader{
		std::size_t contentLength = 0;
		std::string contentType   = "application/vscode-jsonrpc; charset=utf-8";
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

} // namespace lsp
