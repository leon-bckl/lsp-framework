#pragma once

#include <memory>
#include <string>
#include <variant>
#include <lsp/exception.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{
namespace json{
class Value;
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
	using Message = std::variant<jsonrpc::Message, jsonrpc::MessageBatch>;

	Connection(io::Stream& stream);
	~Connection();

	Connection(Connection&&) noexcept;
	Connection& operator=(Connection&&) noexcept;
	Connection(const Connection&)            = delete;
	Connection& operator=(const Connection&) = delete;

	Message readMessage();
	void writeMessage(Message&& message);

private:
	struct Internal;
	std::unique_ptr<Internal> m;

	struct MessageHeader;
	class InputReader;

	MessageHeader readMessageHeader(InputReader& reader);
	static void parseHeaderValue(MessageHeader& header, std::string_view line);
	static void readNextMessageHeaderField(MessageHeader& header, InputReader& reader);
	void writeMessageData(std::string_view content);
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
