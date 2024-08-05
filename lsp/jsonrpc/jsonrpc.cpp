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

	if(jsonrpc.string() != "2.0")
		throw ProtocolError{"Invalid or unsupported jsonrpc version"};
}

MessageId messageIdFromJson(json::Any& json)
{
	if(json.isString())
		return std::move(json.string());

	if(json.isNumber())
		return static_cast<json::Integer>(json.number());

	if(json.isNull())
		return nullptr;

	throw ProtocolError{"Request id type must be string, number or null"};
}

Request requestFromJson(json::Object& json)
{
	verifyProtocolVersion(json);

	Request request;
	request.jsonrpc = std::move(json.get("jsonrpc").string());
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
		auto& error = json.get("error");
		auto& errorObj = error.object();
		auto& responseError = response.error.emplace();

		if(!errorObj.contains("code"))
			throw ProtocolError{"Response error is missing the error code"};

		const auto& errorCode = errorObj.get("code");

		if(!errorCode.isNumber())
			throw ProtocolError{"Response error code must be a number"};

		responseError.code = static_cast<json::Integer>(errorCode.number());

		if(!errorObj.contains("message"))
			throw ProtocolError{"Response error is missing the error message"};

		auto& errorMessage = errorObj.get("message");

		if(!errorMessage.isString())
			throw ProtocolError{"Response error message must be a string"};

		responseError.message = std::move(errorMessage.string());

		if(errorObj.contains("data"))
			responseError.data = errorObj.get("data");
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

	auto firstMessage = messageFromJson(std::move(json[0].object()));

	if(std::holds_alternative<Request>(firstMessage))
	{
		RequestBatch batch;
		batch.reserve(json.size());
		batch.push_back(std::move(std::get<Request>(firstMessage)));
		std::transform(json.begin() + 1, json.end(), std::back_inserter(batch), [](json::Any& v){ return requestFromJson(v.object()); });

		return batch;
	}
	else
	{
		ResponseBatch batch;
		batch.reserve(json.size());
		batch.push_back(std::move(std::get<Response>(firstMessage)));
		std::transform(json.begin() + 1, json.end(), std::back_inserter(batch), [](json::Any& v){ return responseFromJson(v.object()); });

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

Request createRequest(MessageId id, std::string_view method, std::optional<json::Any> params)
{
	Request request;
	request.id = std::move(id);
	request.method = method;
	request.params = std::move(params);
	return request;
}

Request createNotification(std::string_view method, std::optional<json::Any> params)
{
	Request notification;
	notification.method = method;
	notification.params = std::move(params);
	return notification;
}

Response createResponse(MessageId id, json::Any result)
{
	Response response;
	response.id = std::move(id);
	response.result = std::move(result);
	return response;
}

Response createErrorResponse(MessageId id, json::Integer errorCode, json::String message, std::optional<json::Any> data)
{
	Response response;
	response.id = std::move(id);
	auto& error = response.error.emplace();
	error.code = errorCode;
	error.message = std::move(message);
	error.data = std::move(data);
	return response;
}

} // namespace lsp::jsonrpc
