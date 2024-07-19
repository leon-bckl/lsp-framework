#include "jsonrpc.h"

#include <cassert>
#include <iterator>
#include <algorithm>

namespace lsp::jsonrpc{
namespace{

void verifyProtocolVersion(const json::Object& json)
{
	if(!json.contains("jsonrpc"))
		throw ProtocolError{"jsonrpc property is missing"};

	const auto& jsonrpc = json.get("jsonrpc");

	if(!jsonrpc.isString())
		throw ProtocolError{"jsonrpc property expected to be a string"};

	if(jsonrpc.get<json::String>() != "2.0")
		throw ProtocolError{"Invalid or unsupported jsonrpc version"};
}

MessageId messageIdFromJson(json::Any& json)
{
	if(json.isString())
		return std::move(json.get<json::String>());

	if(json.isNumber())
		return static_cast<json::Integer>(json.numberValue());

	if(json.isNull())
		return nullptr;

	throw ProtocolError{"Request id type must be string, number or null"};
}

Request requestFromJson(json::Object& json)
{
	verifyProtocolVersion(json);

	Request request;
	request.jsonrpc = std::move(json.get<json::String>("jsonrpc"));
	request.method = std::move(json.get<json::String>("method"));

	if(json.contains("id"))
		request.id = messageIdFromJson(json.get("id"));

	if(json.contains("params"))
	{
		auto& params = json.get("params");

		if(params.isObject())
			request.params = std::move(params.get<json::Object>());
		else if(params.isArray())
			request.params = std::move(params.get<json::Array>());
		else
			throw ProtocolError{"Params type must be object or array"};
	}

	return request;
}

Response responseFromJson(json::Object& json)
{
	verifyProtocolVersion(json);

	Response response;

	if(json.contains("id"))
		response.id = messageIdFromJson(json.get("id"));

	if(json.contains("result"))
		response.result = std::move(json.get("result"));

	if(json.contains("error"))
	{
		auto& errorJson = json.get<json::Object>("error");
		auto& responseError = response.error.emplace();

		if(!errorJson.contains("code"))
			throw ProtocolError{"Response error is missing the error code"};

		const auto& errorCode = errorJson.get("code");

		if(!errorCode.isNumber())
			throw ProtocolError{"Response error code must be a number"};

		responseError.code = static_cast<json::Integer>(errorCode.numberValue());

		if(!errorJson.contains("message"))
			throw ProtocolError{"Response error is missing the error message"};

		auto& errorMessage = errorJson.get("message");

		if(!errorMessage.isString())
			throw ProtocolError{"Response error message must be a string"};

		responseError.message = std::move(errorMessage.get<json::String>());

		if(errorJson.contains("data"))
			responseError.data = errorJson.get("data");
	}

	if((response.result.has_value() && response.error.has_value()) || (!response.result.has_value() && !response.error.has_value()))
		throw ProtocolError{"Response must have either 'result' or 'error'"};

	return response;
}

} // namespace

std::variant<Request, Response> messageFromJson(json::Object&& json)
{
	if(json.contains("method"))
		return requestFromJson(json);

	return responseFromJson(json);
}

std::variant<RequestBatch, ResponseBatch> messageBatchFromJson(json::Array&& json)
{
	if(json.empty())
		throw ProtocolError{"Message batch must not be empty"};

	auto firstMessage = messageFromJson(std::move(json[0].get<json::Object>()));

	if(std::holds_alternative<Request>(firstMessage))
	{
		RequestBatch batch;
		batch.reserve(json.size());
		batch.push_back(std::move(std::get<Request>(firstMessage)));
		std::transform(json.begin() + 1, json.end(), std::back_inserter(batch), [](json::Any& v){ return requestFromJson(v.get<json::Object>()); });

		return batch;
	}
	else
	{
		ResponseBatch batch;
		batch.reserve(json.size());
		batch.push_back(std::move(std::get<Response>(firstMessage)));
		std::transform(json.begin() + 1, json.end(), std::back_inserter(batch), [](json::Any& v){ return responseFromJson(v.get<json::Object>()); });

		return batch;
	}
}

json::Object requestToJson(Request&& request)
{
	json::Object json;

	assert(request.jsonrpc == "2.0");
	json["jsonrpc"] = std::move(request.jsonrpc);

	if(request.id.has_value())
		std::visit([&json](const auto& v){ json["id"] = std::move(v); }, *request.id);

	json["method"] = std::move(request.method);

	if(request.params.has_value())
		json["params"] = std::move(*request.params);

	return json;
}

json::Object responseToJson(Response&& response)
{
	assert(response.jsonrpc == "2.0");
	assert(response.result.has_value() != response.error.has_value());

	json::Object json;
	json["jsonrpc"] = std::move(response.jsonrpc);
	std::visit([&json](const auto& v){ json["id"] = std::move(v); }, response.id);

	if(response.result.has_value())
		json["result"] = std::move(*response.result);

	if(response.error.has_value())
	{
		auto& responseError = *response.error;
		json::Object errorJson;

		errorJson["code"] = responseError.code;
		errorJson["message"] = std::move(responseError.message);

		if(responseError.data.has_value())
			json["data"] = std::move(*responseError.data);

		json["error"] = std::move(errorJson);
	}

	return json;
}

json::Array requestBatchToJson(RequestBatch&& batch)
{
	json::Array result;
	result.reserve(batch.size());
	std::transform(batch.begin(), batch.end(), std::back_inserter(result), [](auto& r){ return requestToJson(std::move(r)); });
	return result;
}

json::Array responseBatchToJson(ResponseBatch&& batch)
{
	json::Array result;
	result.reserve(batch.size());
	std::transform(batch.begin(), batch.end(), std::back_inserter(result), [](auto& r){ return responseToJson(std::move(r)); });
	return result;
}

Request createRequest(const MessageId& id, std::string_view method, const std::optional<json::Any>& params)
{
	Request request;
	request.id = id;
	request.method = method;
	request.params = params;
	return request;
}

Request createNotification(std::string_view method, const std::optional<json::Any>& params)
{
	Request notification;
	notification.method = method;
	notification.params = params;
	return notification;
}

Response createResponse(const MessageId& id, const json::Any& result)
{
	Response response;
	response.id = id;
	response.result = result;
	return response;
}

Response createErrorResponse(const MessageId& id, json::Integer errorCode, const json::String& message, const std::optional<json::Any>& data)
{
	Response response;
	response.id = id;
	auto& error = response.error.emplace();
	error.code = errorCode;
	error.message = message;
	error.data = data;
	return response;
}

} // namespace lsp::jsonrpc
