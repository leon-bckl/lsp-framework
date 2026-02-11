#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <lsp/exception.h>
#include <lsp/strmap.h>

namespace lsp::json{

/*
 * Types
 */

class Value;
class Object;

using Null    = std::nullptr_t;
using Boolean = bool;
using Integer = std::int32_t;
using Decimal = double;
using String  = std::string;
using Array   = std::vector<Value>;

/*
 * Errors
 */

class Error : public Exception{
protected:
	using Exception::Exception;
};

class TypeError : public Error{
public:
	TypeError(const std::string& message = "Unexpected json value") : Error{message}{}
};

class ParseError : public Error{
public:
	ParseError(const std::string& message, std::size_t textPos)
		: Error{message}
	  , m_textPos{textPos}{}

	std::size_t textPos() const noexcept{ return m_textPos; }

private:
	 std::size_t m_textPos = 0;
};

/*
 * Object
 */

class Object{
public:
	using MapType = StrMap<String, Value>;

	Object();
	Object(const Object& other);
	Object(Object&&) noexcept = default;
	~Object();

	Object& operator=(const Object& other);
	Object& operator=(Object&& other) noexcept = default;

	[[nodiscard]] bool operator==(const Object& other) const;
	[[nodiscard]] bool operator!=(const Object& other) const{ return !(*this == other); }

	[[nodiscard]] Value& operator[](std::string_view key);

	[[nodiscard]] std::size_t size() const;
	[[nodiscard]] bool empty() const;
	[[nodiscard]] bool contains(std::string_view key) const;
	[[nodiscard]] Value& get(std::string_view key);
	[[nodiscard]] const Value& get(std::string_view key) const;
	[[nodiscard]] Value* find(std::string_view key);
	[[nodiscard]] const Value* find(std::string_view key) const;

	[[nodiscard]] MapType& keyValueMap();
	[[nodiscard]] const MapType& keyValueMap() const;

private:
	std::unique_ptr<MapType> m_map;
};

/*
 * Value
 */

class Value{
public:
	using VariantType = std::variant<Null, Boolean, Integer, Decimal, String, Array, Object>;

	constexpr Value() = default;
	constexpr Value(Null){}
	constexpr Value(Boolean b) : m_variant{b}{}
	constexpr Value(Integer i) : m_variant{i}{}
	constexpr Value(Decimal d) : m_variant{d}{}
	Value(String&& s) : m_variant{std::move(s)}{}
	Value(Array&& a) : m_variant{std::move(a)}{}
	Value(Object&& o) : m_variant{std::move(o)}{}

	[[nodiscard]] constexpr bool isNull()    const{ return std::holds_alternative<Null>(m_variant); }
	[[nodiscard]] constexpr bool isBoolean() const{ return std::holds_alternative<Boolean>(m_variant); }
	[[nodiscard]] constexpr bool isInteger() const{ return std::holds_alternative<Integer>(m_variant); }
	[[nodiscard]] constexpr bool isDecimal() const{ return std::holds_alternative<Decimal>(m_variant); }
	[[nodiscard]] constexpr bool isNumber()  const{ return isInteger() || isDecimal(); }
	[[nodiscard]] constexpr bool isString()  const{ return std::holds_alternative<String>(m_variant); }
	[[nodiscard]] constexpr bool isObject()  const{ return std::holds_alternative<Object>(m_variant); }
	[[nodiscard]] constexpr bool isArray()   const{ return std::holds_alternative<Array>(m_variant); }

	[[nodiscard]] Boolean       boolean() const{ return get<Boolean>(); }
	[[nodiscard]] Integer       integer() const{ return get<Integer>(); }
	[[nodiscard]] Decimal       decimal() const{ return get<Decimal>(); }
	[[nodiscard]] const String& string()  const{ return get<String>(); }
	[[nodiscard]] const Object& object()  const{ return get<Object>(); }
	[[nodiscard]] const Array&  array()   const{ return get<Array>(); }
	[[nodiscard]] String&       string(){ return get<String>(); }
	[[nodiscard]] Object&       object(){ return get<Object>(); }
	[[nodiscard]] Array&        array(){ return get<Array>(); }

	[[nodiscard]] Decimal number() const
	{
		if(isDecimal())
			return get<Decimal>();

		if(isInteger())
			return static_cast<Decimal>(get<Integer>());

		throw TypeError{};
	}

	[[nodiscard]] bool operator==(const Value& other) const = default;
	[[nodiscard]] bool operator!=(const Value& other) const = default;

	[[nodiscard]] const VariantType& variant() const{ return m_variant; }
	[[nodiscard]] VariantType& variant(){ return m_variant; }

private:
	VariantType m_variant;

	template<typename T>
	T& get()
	{
		if(auto* const v = std::get_if<T>(&m_variant))
			return *v;

		throw TypeError{};
	}

	template<typename T>
	const T& get() const
	{
		if(auto* const v = std::get_if<T>(&m_variant))
			return *v;

		throw TypeError{};
	}
};

using Any [[deprecated("Use json::Value")]] = Value;

/*
 * parse/stringify
 */

Value       parse(std::string_view text);
std::string stringify(const Value& json, bool format = false);
std::string toStringLiteral(std::string_view str);
std::string fromStringLiteral(std::string_view str);

} // namespace lsp::json
