#pragma once

#include <optional>
#include <stdexcept>
#include <lsp/json/json.h>

#ifndef LSP_ERROR_DONT_INCLUDE_GENERATED_TYPES
#include <lsp/types.h>
#endif

namespace lsp{

// Generated tyeps
class ErrorCodes;
class LSPErrorCodes;

/*
 * Generic base for request or response errors
 */
class Error : public std::runtime_error{
public:
	json::Integer code() const{ return m_code; }
	const std::optional<json::Any>& data() const{ return m_data; }

protected:
	Error(json::Integer code, const std::string& message, std::optional<json::Any> data = {})
		: std::runtime_error{message},
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
	RequestError(ErrorCodes code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

	RequestError(LSPErrorCodes code, const std::string& message, std::optional<json::Any> data = {})
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
	ResponseError(ErrorCodes code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}

	ResponseError(LSPErrorCodes code, const std::string& message, std::optional<json::Any> data = {})
		: Error{code, message, std::move(data)}
	{
	}
#endif
};

} // namespace lsp
