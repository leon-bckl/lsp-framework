#pragma once

#include <optional>
#include <string_view>
#include <lsp/json/json.h>

namespace lsp::jsonrpc{

	using Id = std::variant<json::String, json::Number, json::Null>;
	using Parameters = std::variant<json::Object, json::Array>;

	struct Request{
		bool isNotification() const{ return !id.has_value(); }
		std::string               jsonrpc;
		std::string               method;
		std::optional<Parameters> params;
		std::optional<Id>         id;
	};

	Request requestFromJson(const json::Value& json);

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
		const std::string          jsonrpc = "2.0";
		Id                         id;
		std::optional<json::Value> result;
		std::optional<Error>       error;
	};

	json::Value responseToJson(const Response& response);

}
