#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <variant>
#include <stdexcept>
#include <unordered_map>

namespace lsp::json{

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
};

class ParseError : public std::runtime_error{
public:
	ParseError(const std::string& message, std::size_t textPos) : std::runtime_error{message},
	                                                                  m_textPos{textPos}{}

	std::size_t textPos() const noexcept{ return m_textPos; }

private:
	 std::size_t m_textPos = 0;
};

Value parse(std::string_view text);
std::string stringify(const Value& json);

}
