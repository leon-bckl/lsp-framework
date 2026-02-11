#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <lsp/exception.h>
#include <lsp/json/json.h>

namespace lsp::jsonrpc{

using MessageId = std::variant<json::Null, json::Integer, json::String>;

/*
 * Request
 */

struct Request{
	std::optional<MessageId>   id     = {};
	std::string                method;
	std::optional<json::Value> params = {};

	[[nodiscard]] bool isNotification() const{ return !id.has_value(); }
};

using RequestBatch = std::vector<Request>;
using SingleRequestOrBatch = std::variant<Request, RequestBatch>;

/*
 * Error
 */

struct Error{
	static constexpr json::Integer ParseError     = -32700;
	static constexpr json::Integer InvalidRequest = -32600;
	static constexpr json::Integer MethodNotFound = -32601;
	static constexpr json::Integer InvalidParams  = -32602;
	static constexpr json::Integer InternalError  = -32603;

	json::Integer              code;
	json::String               message;
	std::optional<json::Value> data = {};
};

/*
 * Response
 */

struct Response{
	MessageId                  id;
	std::optional<json::Value> result = {};
	std::optional<Error>       error  = {};
};

using ResponseBatch = std::vector<Response>;
using SingleResponseOrBatch = std::variant<Response, ResponseBatch>;

/*
 * Message
 */

using Message      = std::variant<Request, Response>;
using MessageBatch = std::vector<Message>;

/*
 * Error thrown when a message has an invalid structure
 */

class ProtocolError : public Exception{
public:
	using Exception::Exception;
};

/*
 * Creation/Parsing/Serialization
 */

[[nodiscard]] Message      messageFromJson(json::Object&& json);
[[nodiscard]] MessageBatch messageBatchFromJson(json::Array&& json);
[[nodiscard]] json::Object messageToJson(Message&& message);
[[nodiscard]] json::Array  messageBatchToJson(MessageBatch&& batch);

[[nodiscard]] Request  createRequest(MessageId id, std::string_view method, std::optional<json::Value> params = std::nullopt);
[[nodiscard]] Request  createNotification(std::string_view method, std::optional<json::Value> params = std::nullopt);
[[nodiscard]] Response createResponse(MessageId id, json::Value result);
[[nodiscard]] Response createErrorResponse(MessageId id, json::Integer errorCode, json::String message, std::optional<json::Value> data = std::nullopt);

} // namespace lsp::jsonrpc
