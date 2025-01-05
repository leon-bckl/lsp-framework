#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>
#include <stdexcept>
#include <string_view>
#include <lsp/str.h>

namespace lsp::json{

/*
 * Types
 */

class Any;

using Null    = std::nullptr_t;
using Boolean = bool;
using Decimal = double;
using Integer = std::int32_t;
using String  = std::string;
using Array   = std::vector<Any>;

class TypeError : public std::bad_variant_access{
public:
	TypeError() = default;
	TypeError(std::string message) : m_message{std::move(message)}{}

	const char* what() const noexcept override{ return m_message.c_str(); }

private:
	std::string m_message{"Unexpected json value"};
};

using ObjectMap = str::UnorderedMap<String, Any>;
class Object : public ObjectMap{
public:
	using ObjectMap::ObjectMap;

	Any& get(std::string_view key);
	const Any& get(std::string_view key) const;
};

using AnyVariant = std::variant<Null, Boolean, Integer, Decimal, String, Object, Array>;
class Any : private AnyVariant{
	using AnyVariant::AnyVariant;
public:
	Any() : AnyVariant{nullptr}{}

	bool isNull() const{ return std::holds_alternative<Null>(*this); }
	bool isBoolean() const{ return std::holds_alternative<Boolean>(*this); }
	bool isInteger() const{ return std::holds_alternative<Integer>(*this); }
	bool isDecimal() const{ return std::holds_alternative<Decimal>(*this); }
	bool isNumber() const{ return isInteger() || isDecimal(); }
	bool isString() const{ return std::holds_alternative<String>(*this); }
	bool isObject() const{ return std::holds_alternative<Object>(*this); }
	bool isArray() const{ return std::holds_alternative<Array>(*this); }

	Boolean boolean() const{ return get<Boolean>(); }
	Integer integer() const{ return get<Integer>(); }
	Decimal decimal() const{ return get<Decimal>(); }
	const String& string() const{ return get<String>(); }
	String& string(){ return get<String>(); }
	const Object& object() const{ return get<Object>(); }
	Object& object(){ return get<Object>(); }
	const Array& array() const{ return get<Array>(); }
	Array& array(){ return get<Array>(); }

	Decimal number() const
	{
		if(isDecimal())
			return get<Decimal>();

		if(isInteger())
			return static_cast<Decimal>(get<Integer>());

		throw TypeError{};
	}

private:
	template<typename T>
	T& get()
	{
		if(std::holds_alternative<T>(*this))
			return std::get<T>(*this);

		throw TypeError{};
	}

	Any& get()
	{
		return *this;
	}

	template<typename T>
	const T& get() const
	{
		if(std::holds_alternative<T>(*this))
			return std::get<T>(*this);

		throw TypeError{};
	}

	const Any& get() const
	{
		return *this;
	}
};

/*
 * parse/stringify
 */

class ParseError : public std::runtime_error{
public:
	ParseError(const std::string& message, std::size_t textPos) : std::runtime_error{message},
	                                                              m_textPos{textPos}{}

	std::size_t textPos() const noexcept{ return m_textPos; }

private:
	 std::size_t m_textPos = 0;
};

Any parse(std::string_view text);
std::string stringify(const Any& json, bool format = false);

} // namespace lsp::json
