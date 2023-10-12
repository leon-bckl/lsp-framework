#pragma once

#include <optional>
#include <json/json.h>
#include <string_view>

namespace jsonrpc{
	using Id = std::variant<json::String, json::Number, json::Null>;
	using Parameters = std::variant<json::Object, json::Array>;

	struct Request{
		bool isNotification() const{ return !id.has_value(); }

		json::String              jsonrpc = "2.0";
		json::String              method;
		std::optional<Parameters> params;
		std::optional<Id>         id;
	};

	Request parseRequest(std::string_view jsonText);

	enum class ErrorCode{
		ParseError     = -32700,
		InvalidRequest = -32600,
		MethodNotFound = -32601,
		InvalidParams  = -32602,
		InternalError  = -32603
	};

	struct Error{
		int                        code;
		json::String               message;
		std::optional<json::Value> data;
	};

	struct Response{
		const json::String         jsonrpc = "2.0";
		Id                         id;
		std::optional<json::Value> result;
		std::optional<Error>       error;
	};

	std::string responseToJsonString(const Response& response);
}
