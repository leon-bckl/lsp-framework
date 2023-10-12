#pragma once

#include <optional>
#include <json/json.h>

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
		json::String               jsonrpc = "2.0";
		std::optional<json::Value> result;
		std::optional<json::Value> error;
		Id                         id;
	};
}
