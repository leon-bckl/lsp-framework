#include <cassert>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp::jsonrpc{
namespace{

constexpr auto ProtocolVersion = std::string_view("2.0");

void verifyProtocolVersion(const json::Object& json)
{
	if(!json.contains("jsonrpc"))
		throw ProtocolError("jsonrpc property is missing");

	const auto& jsonrpc = json.get("jsonrpc");

	if(!jsonrpc.isString())
		throw ProtocolError("jsonrpc property expected to be a string");

	if(jsonrpc.string() != ProtocolVersion)
		throw ProtocolError("Invalid or unsupported jsonrpc version");
}

auto messageIdFromJson(json::Value& json) -> MessageId
{
	if(json.isString())
		return std::move(json.string());

	if(json.isNumber())
		return static_cast<json::Integer>(json.number());

	if(json.isNull())
		return nullptr;

	throw ProtocolError("Request id type must be string, number or null");
}

auto requestFromJson(json::Object& json) -> Request
{
	verifyProtocolVersion(json);

	auto request = Request();
	request.method = std::move(json.get("method").string());

	if(json.contains("id"))
		request.id = messageIdFromJson(json.get("id"));

	if(json.contains("params"))
	{
		auto& params = json.get("params");

		if(params.isObject())
			request.params = std::move(params.object());
		else if(params.isArray())
			request.params = std::move(params.array());
		else if(!params.isNull()) // Be lenient and allow null params even though it is not allowed by jsonrpc 2.0
			throw ProtocolError("Params type must be object or array");
	}

	return request;
}

auto responseFromJson(json::Object& json) -> Response
{
	verifyProtocolVersion(json);

	auto response = Response();

	if(json.contains("id"))
		response.id = messageIdFromJson(json.get("id"));

	if(json.contains("result"))
		response.result = std::move(json.get("result"));

	if(json.contains("error"))
	{
		auto& error         = json.get("error");
		auto& errorObj      = error.object();
		auto& responseError = response.error.emplace();

		if(!errorObj.contains("code"))
			throw ProtocolError("Response error is missing the error code");

		const auto& errorCode = errorObj.get("code");

		if(!errorCode.isNumber())
			throw ProtocolError("Response error code must be a number");

		responseError.code = static_cast<json::Integer>(errorCode.number());

		if(!errorObj.contains("message"))
			throw ProtocolError("Response error is missing the error message");

		auto& errorMessage = errorObj.get("message");

		if(!errorMessage.isString())
			throw ProtocolError("Response error message must be a string");

		responseError.message = std::move(errorMessage.string());

		if(errorObj.contains("data"))
			responseError.data = errorObj.get("data");
	}

	if((response.result.has_value() && response.error.has_value()) || (!response.result.has_value() && !response.error.has_value()))
		throw ProtocolError("Response must have either 'result' or 'error'");

	return response;
}

} // namespace

auto messageFromJson(json::Object&& json) -> Message
{
	if(json.contains("method"))
		return requestFromJson(json);

	return responseFromJson(json);
}

auto messageBatchFromJson(json::Array&& json) -> MessageBatch
{
	if(json.empty())
		throw ProtocolError("Message batch must not be empty");

	auto batch = MessageBatch();
	batch.reserve(json.size());

	for(auto& jsonMessage : json)
		batch.push_back(messageFromJson(std::move(jsonMessage.object())));

	return batch;
}

} // namespace lsp::jsonrpc
