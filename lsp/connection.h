#pragma once

#include <memory>
#include <string>
#include <variant>
#include <lsp/exception.h>
#include <lsp/json/writer.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/jsonrpc/message_writer.h>
#include <lsp/serialization.h>

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

	/*
	 * MessageSender
	 */

	class MessageSender{
	public:
		void discard();

	protected:
		MessageSender(Connection& connection);
		~MessageSender();

		json::Writer&    writer();
		void             submit();

	private:
		Connection*  m_connection = nullptr;
		std::string  m_buffer;
		json::Writer m_writer;
	};

	/*
	 * RequestSender
	 */

	class RequestSender : public MessageSender, public jsonrpc::RequestWriter{
		friend class Connection;
	public:
		void submit();

		template<typename T>
		void writeParams(const T& value)
		{
			jsonrpc::RequestWriter::writeParams(
				[](std::string_view key, const T& value, json::ObjectWriter& writer)
				{
					writeJson(key, value, writer);
				}, value);
		}

	private:
		RequestSender(Connection& connection, std::string_view method, const jsonrpc::MessageId& id);
		RequestSender(Connection& connection, std::string_view method);
	};

	/*
	 * ResponseSender
	 */

	class ResponseSender : public MessageSender, public jsonrpc::ResponseWriter{
		friend class Connection;
	public:
		void submit();

		template<typename T>
		void writeData(const T& value)
		{
			jsonrpc::ResponseWriter::writeData(
				[](std::string_view key, const T& value, json::ObjectWriter& writer)
				{
					writeJson(key, value, writer);
				}, value);
		}

	private:
		ResponseSender(Connection& connection, const jsonrpc::MessageId& id);
		ResponseSender(Connection& connection, const jsonrpc::MessageId& id, int code, std::string_view message);
	};

	/*
	 * BatchSender
	 */

	class BatchSender : public MessageSender, public jsonrpc::BatchWriter{
		friend class Connection;
	public:
		void submit();

	private:
		BatchSender(Connection& connection);
	};

	[[nodiscard]] RequestSender  request(std::string_view method, const jsonrpc::MessageId& id);
	[[nodiscard]] RequestSender  notification(std::string_view method);
	[[nodiscard]] ResponseSender response(const jsonrpc::MessageId& id);
	[[nodiscard]] ResponseSender errorResponse(const jsonrpc::MessageId& id, int code, std::string_view message);
	[[nodiscard]] BatchSender    messageBatch();

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
