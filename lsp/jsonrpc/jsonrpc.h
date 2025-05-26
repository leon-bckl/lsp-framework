#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <lsp/exception.h>
#include <lsp/json/json.h>

namespace lsp::jsonrpc{

using MessageId = std::variant<json::String, json::Integer, json::Null>;

/*
 * Request
 */

struct Request{
	std::optional<MessageId> id     = {};
	std::string              method;
	std::optional<json::Any> params = {};

	bool isNotification() const{ return !id.has_value(); }
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

	json::Integer            code;
	json::String             message;
	std::optional<json::Any> data = {};
};

/*
 * Response
 */

struct Response{
	MessageId                id;
	std::optional<json::Any> result = {};
	std::optional<Error>     error  = {};
};

using ResponseBatch = std::vector<Response>;
using SingleResponseOrBatch = std::variant<Response, ResponseBatch>;

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
