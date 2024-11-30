#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <lsp/json/json.h>

namespace lsp::jsonrpc{

	struct Message{
		std::string jsonrpc = "2.0";

		virtual ~Message() = default;
		virtual bool isRequest() const{ return false; }
		virtual bool isResponse() const{ return false; }
	};

	using MessageId = std::variant<json::String, json::Integer, json::Null>;

	/*
	 * Request
	 */

	struct Request final : Message{
		std::optional<MessageId> id;
		std::string              method;
		std::optional<json::Any> params;

		bool isRequest() const override{ return true; }
		bool isNotification() const{ return !id.has_value(); }
	};

	using RequestBatch = std::vector<Request>;

	/*
	 * Response
	 */

	namespace error{

		inline constexpr json::Integer ParseError     = -32700;
		inline constexpr json::Integer InvalidRequest = -32600;
		inline constexpr json::Integer MethodNotFound = -32601;
		inline constexpr json::Integer InvalidParams  = -32602;
		inline constexpr json::Integer InternalError  = -32603;

	} // namespace error

	struct ResponseError{
		json::Integer            code;
		json::String             message;
		std::optional<json::Any> data;
	};

	struct Response final : Message{
		MessageId                    id = json::Null{};
		std::optional<json::Any>     result;
		std::optional<ResponseError> error;

		bool isResponse() const override{ return true; }
	};

	using ResponseBatch = std::vector<Response>;

	/*
	 * Error thrown when a message has an invalid structure
	 */
	class ProtocolError : public std::runtime_error{
	public:
		using std::runtime_error::runtime_error;
	};

	std::variant<Request, Response>           messageFromJson(json::Object&& json);
	std::variant<RequestBatch, ResponseBatch> messageBatchFromJson(json::Array&& json);

	json::Object requestToJson(Request&& request);
	json::Object responseToJson(Response&& response);
	json::Array  requestBatchToJson(RequestBatch&& batch);
	json::Array  responseBatchToJson(ResponseBatch&& batch);

	Request  createRequest(MessageId id, std::string_view method, std::optional<json::Any> params = std::nullopt);
	Request  createNotification(std::string_view method, std::optional<json::Any> params = std::nullopt);
	Response createResponse(MessageId id, json::Any result);
	Response createErrorResponse(MessageId id, json::Integer errorCode, json::String message, std::optional<json::Any> data = std::nullopt);

} // namespace lsp::jsonrpc
