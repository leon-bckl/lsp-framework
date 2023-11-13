#include "jsonrpc.h"

#include <cassert>

namespace lsp::jsonrpc{
namespace{

MessageId messageIdFromJson(const json::Any& json){
	if(json.isString())
		return json.get<json::String>();

	if(json.isNumber())
		return static_cast<json::Integer>(json.numberValue());

	if(json.isNull())
		return nullptr;

	throw ProtocolError{"Request id type must be string, number or null"};
}

RequestPtr requestFromJson(const json::Object& json){
	auto request = std::make_unique<Request>();

	request->jsonrpc = json.get<json::String>("jsonrpc");
	request->method = json.get<json::String>("method");

	if(json.contains("id"))
		request->id = messageIdFromJson(json.get("id"));

	if(json.contains("params")){
		const auto& params = json.get("params");

		if(params.isObject())
			request->params = params.get<json::Object>();
		else if(params.isArray())
			request->params = params.get<json::Array>();
		else
			throw ProtocolError{"Params type must be object or array"};
	}

	return request;
}

ResponsePtr responseFromJson(const json::Object& json){
	auto response = std::make_unique<Response>();

	if(json.contains("id"))
		response->id = messageIdFromJson(json.get("id"));

	if(json.contains("result"))
		response->result = json.get("result");

	if(json.contains("error")){
		const auto& errorJson = json.get<json::Object>("error");
		response->error.emplace();
		auto& responseError = response->error.value();

		if(!errorJson.contains("code"))
			throw ProtocolError{"Response error is missing the error code"};

		const auto& errorCode = errorJson.get("code");

		if(!errorCode.isNumber())
			throw ProtocolError{"Response error code must be a number"};

		responseError.code = static_cast<json::Integer>(errorCode.numberValue());

		if(!errorJson.contains("message"))
			throw ProtocolError{"Response error is missing the error message"};

		const auto& errorMessage = errorJson.get("message");

		if(!errorMessage.isString())
			throw ProtocolError{"Response error message must be a string"};

		responseError.message = errorMessage.get<json::String>();

		if(errorJson.contains("data"))
			responseError.data = errorJson.get("data");
	}

	if((response->result.has_value() && response->error.has_value()) || (!response->result.has_value() && !response->error.has_value()))
		throw ProtocolError{"Response must have either 'result' or 'error'"};

	return response;
}

MessagePtr messageFromJson(const json::Object& json){
	if(!json.contains("jsonrpc"))
		throw ProtocolError{"jsonrpc property is missing"};

	const auto& jsonrpc = json.get("jsonrpc");

	if(!jsonrpc.isString())
		throw ProtocolError{"jsonrpc property expected to be a string"};

	if(jsonrpc.get<json::String>() != "2.0")
		throw ProtocolError{"Invalid or unsupported jsonrpc version"};

	if(json.contains("method"))
		return requestFromJson(json);

	return responseFromJson(json);
}

} // namespace

json::Any Request::toJson() const{
	json::Object json;

	assert(jsonrpc == "2.0");
	json["jsonrpc"] = jsonrpc;

	if(id.has_value())
		std::visit([&json](const auto& v){ json["id"] = v; }, id.value());

	json["method"] = method;

	if(params.has_value())
		json["params"] = params.value();

	return json;
}

json::Any Response::toJson() const{
	assert(jsonrpc == "2.0");
	assert(result.has_value() != error.has_value());

	json::Object json;
	json["jsonrpc"] = jsonrpc;
	std::visit([&json](const auto& v){ json["id"] = v; }, id);

	if(result.has_value())
		json["result"] = result.value();

	if(error.has_value()){
		const auto& responseError = error.value();
		json::Object errorJson;

		errorJson["code"] = responseError.code;
		errorJson["message"] = responseError.message;

		if(responseError.data.has_value())
			json["data"] = responseError.data.value();

		json["error"] = errorJson;
	}

	return json;
}

std::variant<MessagePtr, MessageBatch> messageFromJson(const json::Any& json){
	if(json.isObject())
		return messageFromJson(json.get<json::Object>());

	if(!json.isArray())
		throw ProtocolError{"Message must be either a single object or an array of messages"};

	const auto& array = json.get<json::Array>();
	MessageBatch batch;
	batch.reserve(array.size());

	for(const auto& m : array){
		if(!m.isObject())
			throw ProtocolError{"Message must be an object"};

		batch.push_back(messageFromJson(m.get<json::Object>()));
	}

	return batch;
}

RequestPtr createRequest(const MessageId& id, const std::string& method, const std::optional<json::Any>& params){
	auto request = std::make_unique<Request>();
	request->id = id;
	request->method = method;
	request->params = params;

	return request;
}

RequestPtr createNotification(const std::string& method, const std::optional<json::Any>& params){
	auto notification = std::make_unique<Request>();
	notification->method = method;
	notification->params = params;

	return notification;
}

ResponsePtr createResponse(const MessageId& id, const json::Any& result){
	auto response = std::make_unique<Response>();
	response->id = id;
	response->result = result;

	return response;
}

ResponsePtr createErrorResponse(const MessageId& id, json::Integer errorCode, const json::String& message, const std::optional<json::Any>& data){
	auto response = std::make_unique<Response>();
	response->id = id;

	auto& error = response->error.emplace();
	error.code = errorCode;
	error.message = message;
	error.data = data;

	return response;
}

} // namespace lsp::jsonrpc
