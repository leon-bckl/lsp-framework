#include "jsonrpc.h"

namespace lsp::jsonrpc{
namespace{

json::Value idToJson(const Id& id){
	if(std::holds_alternative<json::String>(id))
		return std::get<json::String>(id);

	if(std::holds_alternative<json::Number>(id))
		return std::get<json::Number>(id);

	return {};
}

json::Object errorToJson(const Error& error){
	json::Object json;

	json["code"] = json::Number(error.code);
	json["message"] = error.message;

	if(error.data.has_value())
		json["data"] = error.data.value();

	return json;
}

}

Request requestFromJson(const json::Value& json){
	Request request;

	if(!std::holds_alternative<json::Object>(json))
		return {}; // ERROR: Expected json object

	const auto& object = std::get<json::Object>(json);
	auto it = object.find("jsonrpc");

	if(it == object.end() || !std::holds_alternative<json::String>(it->second) || std::get<json::String>(it->second) != "2.0")
		return {}; // ERROR: Unsupported or missing jsonrpc version

	request.jsonrpc = std::get<json::String>(it->second);

	it = object.find("method");

	if(it == object.end() || !std::holds_alternative<json::String>(it->second))
		return {}; // ERROR: Missing or invalid method

	request.method = std::get<json::String>(it->second);

	it = object.find("id");

	if(it != object.end()){
		if(it->second.isString())
			request.id = std::get<json::String>(it->second);
		else if(it->second.isNumber())
			request.id = std::get<json::Number>(it->second);
		else if(it->second.isNull())
			request.id = json::Null{};
		// else: ERROR: Id must be string, number or null
	}

	it = object.find("params");

	if(it != object.end()){
		if(it->second.isObject())
			request.params = std::get<json::Object>(it->second);
		else if(it->second.isArray())
			request.params = std::get<json::Array>(it->second);
		// else: ERROR: params must be object or array
	}

	return request;
}

json::Value responseToJson(const Response& response){
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
