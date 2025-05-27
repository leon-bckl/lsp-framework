#pragma once

#include <optional>
#include <lsp/types.h>
#include <lsp/exception.h>
#include <lsp/json/json.h>

namespace lsp{

/*
 * Generic base for request or response errors
 */
class Error : public Exception{
public:
	json::Integer code() const{ return m_code; }
	const std::optional<json::Any>& data() const{ return m_data; }

protected:
	Error(json::Integer code, const std::string& message, std::optional<json::Any> data = {})
		: Exception{message},
		  m_code{code},
		  m_data{std::move(data)}
	{
	}

private:
	json::Integer            m_code;
	std::optional<json::Any> m_data;
};

/*
 * Thrown by the implementation if it encounters an error when processing a request
 */
class RequestError : public Error{
public:
	RequestError(json::Integer code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

#ifndef LSP_ERROR_DONT_INCLUDE_GENERATED_TYPES
	RequestError(ErrorCodesEnum code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

	RequestError(LSPErrorCodesEnum code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}
#endif
};

/*
 * Thrown when attempting to get the result value of a request but an error response was received
 */
class ResponseError : public Error{
public:
	ResponseError(json::Integer code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

#ifndef LSP_ERROR_DONT_INCLUDE_GENERATED_TYPES
	ResponseError(ErrorCodesEnum code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

	ResponseError(LSPErrorCodesEnum code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}
#endif
};

} // namespace lsp
