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
	Error(const std::string& message, json::Integer code, std::optional<json::Any> data) :
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
	RequestError(const std::string& message, types::ErrorCodes code, std::optional<json::Any> data = std::nullopt) :
		Error{message, code, std::move(data)}{}
	RequestError(const std::string& message, types::LSPErrorCodes code, std::optional<json::Any> data = std::nullopt) :
		Error{message, code, std::move(data)}{}
};

/*
 * Thrown when attempting to get the result value of a request but an error response was received
 */
class ResponseError : public Error{
public:
	ResponseError(const std::string& message, types::ErrorCodes code, std::optional<json::Any> data = std::nullopt) :
		Error{message, code, std::move(data)}{}
	ResponseError(const std::string& message, types::LSPErrorCodes code, std::optional<json::Any> data = std::nullopt) :
		Error{message, code, std::move(data)}{}
};

} // namespace lsp
