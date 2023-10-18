#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <variant>
#include <stdexcept>
#include <unordered_map>

namespace lsp::json{

/*
 * Types
 */

class Value;

using Null    = std::nullptr_t;
using Boolean = bool;
using Number  = double;
using String  = std::string;
using Object  = std::unordered_map<String, Value>;
using Array   = std::vector<Value>;

using ValueVariant = std::variant<Null, Boolean, Number, String, Object, Array>;
class Value : public ValueVariant{
	using ValueVariant::ValueVariant;
public:
	Value() : ValueVariant{nullptr}{}

	bool isNull() const{ return std::holds_alternative<Null>(*this); }
	bool isBoolean() const{ return std::holds_alternative<Boolean>(*this); }
	bool isNumber() const{ return std::holds_alternative<Number>(*this); }
	bool isString() const{ return std::holds_alternative<String>(*this); }
	bool isObject() const{ return std::holds_alternative<Object>(*this); }
	bool isArray() const{ return std::holds_alternative<Array>(*this); }
};

/*
 * Exceptions
 */

class ParseError : public std::runtime_error{
public:
	ParseError(const std::string& message, std::size_t textPos) : std::runtime_error{message},
	                                                              m_textPos{textPos}{}

	std::size_t textPos() const noexcept{ return m_textPos; }

private:
	 std::size_t m_textPos = 0;
};

/*
 * parse/stringify
 */

Value parse(std::string_view text);
std::string stringify(const Value& json);

}
