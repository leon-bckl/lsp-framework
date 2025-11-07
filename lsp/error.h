#pragma once

#include <optional>
#include <lsp/exception.h>
#include <lsp/json/json.h>

namespace lsp{

/*
 * Generic base for request or response errors
 */
class MessageError : public Exception{
public:
	const char* message() const noexcept{ return what(); }
	int code() const noexcept{ return m_code; }
	const std::optional<json::Any>& data() const noexcept{ return m_data; }

	enum : int{
		ParseError           = -32700,
		InvalidRequest       = -32600,
		MethodNotFound       = -32601,
		InvalidParams        = -32602,
		InternalError        = -32603,
		ServerNotInitialized = -32002,
		UnknownErrorCode     = -32001,
		RequestFailed        = -32803,
		ServerCancelled      = -32802,
		ContentModified      = -32801,
		RequestCancelled     = -32800
	};

protected:
	MessageError(int code, const std::string& message, std::optional<json::Any> data = {})
		: Exception{message},
		  m_code{code},
		  m_data{std::move(data)}
	{
	}

private:
	int                      m_code;
	std::optional<json::Any> m_data;
};
using Error [[deprecated]] = MessageError;

/*
 * Thrown from inside of a request handler callback and sent back as an error response
 */
class RequestError : public MessageError{
public:
	RequestError(int code, const std::string& message, std::optional<json::Any> data = {})
		: MessageError{code, message, std::move(data)}
	{
	}
};

/*
 * Thrown when attempting to get the result value of a request but an error response was received
 */
class ResponseError : public MessageError{
public:
	ResponseError(int code, const std::string& message, std::optional<json::Any> data = {})
		: MessageError{code, message, std::move(data)}
	{
	}
};

} // namespace lsp
