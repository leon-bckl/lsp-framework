#pragma once

#include <optional>
#include <stdexcept>
#include <lsp/types.h>
#include <lsp/json/json.h>

namespace lsp{

/*
 * Generic base for request or response errors
 */
class Error : public std::runtime_error{
public:
	json::Integer code() const{ return m_code; }
	const std::optional<json::Any>& data() const{ return m_data; }

protected:
	Error(json::Integer code, const std::string& message, std::optional<json::Any> data) :
		std::runtime_error{message},
	  m_code{code},
		m_data{std::move(data)}{}

private:
	json::Integer            m_code;
	std::optional<json::Any> m_data;
};

/*
 * Thrown by the implementation if it encounters an error when processing a request
 */
class RequestError : public Error{
public:
	RequestError(ErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt) :
		Error{code, message, std::move(data)}{}
	RequestError(LSPErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt) :
		Error{code, message, std::move(data)}{}
};

/*
 * Thrown when attempting to get the result value of a request but an error response was received
 */
class ResponseError : public Error{
public:
	ResponseError(ErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt) :
		Error{code, message, std::move(data)}{}
	ResponseError(LSPErrorCodes code, const std::string& message, std::optional<json::Any> data = std::nullopt) :
		Error{code, message, std::move(data)}{}
};

} // namespace lsp
