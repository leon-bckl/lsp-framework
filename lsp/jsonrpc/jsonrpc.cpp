#include "jsonrpc.h"

namespace lsp::jsonrpc{
namespace{

json::Any idToJson(const Id& id){
	if(std::holds_alternative<json::String>(id))
		return std::get<json::String>(id);

	if(std::holds_alternative<json::Integer>(id))
		return std::get<json::Integer>(id);

	return {};
}

json::Object errorToJson(const Error& error){
	json::Object json;

	json["code"] = json::Integer{error.code};
	json["message"] = error.message;

	if(error.data.has_value())
		json["data"] = error.data.value();

	return json;
}

}

Request requestFromJson(const json::Any& json){
	Request request;

	const auto& object = json.get<json::Object>();
	auto it = object.find("jsonrpc");

	if(it == object.end() || !it->second.isString() || it->second.get<json::String>() != "2.0")
		return {}; // ERROR: Unsupported or missing jsonrpc version

	request.jsonrpc = it->second.get<json::String>();

	it = object.find("method");

	if(it == object.end() || !it->second.isString())
		return {}; // ERROR: Missing or invalid method

	request.method = it->second.get<json::String>();

	it = object.find("id");

	if(it != object.end()){
		if(it->second.isString())
			request.id = it->second.get<json::String>();
		else if(it->second.isNumber())
			request.id = static_cast<json::Integer>(it->second.numberValue());
		else if(it->second.isNull())
			request.id = json::Null{};
		// else: ERROR: Id must be string, number or null
	}

	it = object.find("params");

	if(it != object.end()){
		if(it->second.isObject())
			request.params = it->second.get<json::Object>();
		else if(it->second.isArray())
			request.params = it->second.get<json::Array>();
		// else: ERROR: params must be object or array
	}

	return request;
}

json::Any responseToJson(const Response& response){
	json::Object json;

	json["jsonrpc"] = response.jsonrpc;
	json["id"] = idToJson(response.id);

	if(response.result.has_value())
		json["result"] = response.result.value();

	if(response.error.has_value())
		json["error"] = errorToJson(response.error.value());

	return json;
}

}
