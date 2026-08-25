#pragma once

#include <memory>
#include <string>
#include <variant>
#include <lsp/exception.h>
#include <lsp/json/writer.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/jsonrpc/messagewriter.h>
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
	void writeMessage(Message&& message);

	/*
	 * MessageSender
	 */

	class MessageSender{
	protected:
		MessageSender();

		std::string_view buffer();
		json::Writer&    writer();

	private:
		std::string  m_buffer;
		json::Writer m_writer;
	};

	/*
	 * RequestSender
	 */

	class RequestSender : public MessageSender, public jsonrpc::RequestWriter{
		friend class Connection;
	public:
		void submit(Connection& connection);

		template<typename T>
		void writeParams(const T& value)
		{
			if constexpr(json::JsonPrimitive<T>)
			{
				writer().write(value);
			}
			else if constexpr(impl::IsVector<T>{} || impl::IsTuple<T>{})
			{
				auto arrayWriter = writeParamsArray();
				toJson(value, arrayWriter);
			}
			else
			{
				auto objectWriter = writeParamsObject();
				toJson(value, objectWriter);
			}
		}

	private:
		RequestSender(std::string_view method, const jsonrpc::MessageId& id);
		RequestSender(std::string_view method);
	};

	/*
	 * ResponseSender
	 */

	class ResponseSender : public MessageSender, public jsonrpc::ResponseWriter{
		friend class Connection;
	public:
		void submit(Connection& connection);

	private:
		ResponseSender(const jsonrpc::MessageId& id);
		ResponseSender(const jsonrpc::MessageId& id, int code, std::string_view message);
	};

	/*
	 * BatchSender
	 */

	class BatchSender : public MessageSender, public jsonrpc::BatchWriter{
		friend class Connection;
	public:
		void submit(Connection& connection);

	private:
		BatchSender();
	};

	[[nodiscard]] static RequestSender  request(std::string_view method, const jsonrpc::MessageId& id);
	[[nodiscard]] static RequestSender  notification(std::string_view method);
	[[nodiscard]] static ResponseSender response(const jsonrpc::MessageId& id);
	[[nodiscard]] static ResponseSender errorResponse(const jsonrpc::MessageId& id, int code, std::string_view message);
	[[nodiscard]] static BatchSender    messageBatch();

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
