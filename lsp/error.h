#pragma once

#include <optional>
#include <stdexcept>
#include <lsp/types.h>
#include <lsp/serialization.h>

namespace lsp{

class ErrorCode{
public:
	ErrorCode(ErrorCodes code)
		: m_code{code.value()}
	{
	}

	ErrorCode(LSPErrorCodes code)
		: m_code{code.value()}
	{
	}

	auto value() const -> json::Integer{ return m_code; }
	operator json::Integer() const{ return value(); }

private:
	json::Integer m_code;
};

/*
 * Generic base for request or response errors
 */
class Error : public std::runtime_error{
public:
	json::Integer code() const{ return m_code; }
	const std::optional<json::Any>& data() const{ return m_data; }

protected:
	Error(json::Integer code, const std::string& message, std::optional<json::Any> data = std::nullopt)
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
	RequestError(ErrorCodes code, const std::string& message)
		: Error{code, message}
	{
	}
	RequestError(LSPErrorCodes code, const std::string& message)
		: Error{code, message}
	{
	}

	template<typename ErrorCodeType, typename ErrorData>
	RequestError(ErrorCodeType code, const std::string& message, ErrorData&& data)
		: Error{ErrorCode{code}, message, toJson(std::forward<ErrorData>(data))}
	{
	}
};

/*
 * Thrown when attempting to get the result value of a request but an error response was received
 */
class ResponseError : public Error{
public:
	ResponseError(ErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt)
		: Error{code, message, std::move(data)}
	{
	}

	ResponseError(LSPErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt)
		: Error{code, message, std::move(data)}
	{
	}
};

} // namespace lsp
