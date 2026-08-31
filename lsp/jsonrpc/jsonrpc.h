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

	[[nodiscard]] auto isNotification() const -> bool{ return !id.has_value(); }
};

using RequestBatch = std::vector<Request>;
using SingleRequestOrBatch = std::variant<Request, RequestBatch>;

/*
 * Error
 */

struct Error{
	enum : int{
		ParseError     = -32700,
		InvalidRequest = -32600,
		MethodNotFound = -32601,
		InvalidParams  = -32602,
		InternalError  = -32603
	};

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

[[nodiscard]] auto messageFromJson(json::Object&& json) -> Message;
[[nodiscard]] auto messageBatchFromJson(json::Array&& json) -> MessageBatch;

} // namespace lsp::jsonrpc
