#pragma once

#include <utility>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <lsp/exception.h>

namespace lsp::json{

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
 * parse/stringify
 */

Value  parse(std::string_view text);
String stringify(const Value& json, std::string_view indent = {});
String toStringLiteral(std::string_view str);
String fromStringLiteral(std::string_view str);

/*
 * Object
 */

class Object{
public:
	using SizeType = std::size_t;
	class KeyValuePair;

	Object();
	Object(std::initializer_list<KeyValuePair> pairs);
	Object(const Object& other);
	Object(Object&&) noexcept;
	~Object();

	Object& operator=(const Object& other);
	Object& operator=(Object&& other) noexcept;

	[[nodiscard]] SizeType size() const;
	[[nodiscard]] SizeType capacity() const;
	[[nodiscard]] bool     isEmpty() const{ return size() == 0; }

	Value& insert(String key, Value value);
	void   remove(std::string_view key);
	void   clear();
	void   reserve(SizeType size);

	[[nodiscard]] Value*       find(std::string_view key);
	[[nodiscard]] const Value* find(std::string_view key) const{ return const_cast<Object*>(this)->find(key); }
	[[nodiscard]] bool         contains(std::string_view key) const{ return find(key) != nullptr; }
	[[nodiscard]] Value&       get(std::string_view key);
	[[nodiscard]] const Value& get(std::string_view key) const{ return const_cast<Object*>(this)->get(key); }

	[[nodiscard]] Value& operator[](std::string_view key);

	[[nodiscard]] bool operator==(const Object& other) const;
	[[nodiscard]] bool operator!=(const Object& other) const{ return !(*this == other); }

	[[nodiscard]] auto begin();
	[[nodiscard]] auto begin() const;
	[[nodiscard]] auto cbegin() const;
	[[nodiscard]] auto end();
	[[nodiscard]] auto end() const;
	[[nodiscard]] auto cend() const;

private:
	std::vector<KeyValuePair> m_keyValuePairs; // lsp objects are quite small so a map would be overkill. We can always add one on top of this list...
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
	Value(const char* s)       : m_variant{String(s)}{}
	Value(std::string_view s)  : m_variant{String(s)}{}
	Value(String&& s)          : m_variant{std::move(s)}{}
	Value(Array&& a)           : m_variant{std::move(a)}{}
	Value(Object&& o)          : m_variant{std::move(o)}{}

	[[nodiscard]] constexpr bool isNull()    const{ return std::holds_alternative<Null>(m_variant); }
	[[nodiscard]] constexpr bool isBoolean() const{ return std::holds_alternative<Boolean>(m_variant); }
	[[nodiscard]] constexpr bool isInteger() const{ return std::holds_alternative<Integer>(m_variant); }
	[[nodiscard]] constexpr bool isDecimal() const{ return std::holds_alternative<Decimal>(m_variant); }
	[[nodiscard]] constexpr bool isNumber()  const{ return isInteger() || isDecimal(); }
	[[nodiscard]] constexpr bool isString()  const{ return std::holds_alternative<String>(m_variant); }
	[[nodiscard]] constexpr bool isObject()  const{ return std::holds_alternative<Object>(m_variant); }
	[[nodiscard]] constexpr bool isArray()   const{ return std::holds_alternative<Array>(m_variant); }

	[[nodiscard]] Boolean       boolean() const{ return get<Boolean>("boolean"); }
	[[nodiscard]] Integer       integer() const{ return get<Integer>("integer"); }
	[[nodiscard]] Decimal       decimal() const{ return get<Decimal>("decimal"); }
	[[nodiscard]] const String& string()  const{ return get<String>("string"); }
	[[nodiscard]] const Object& object()  const{ return get<Object>("object"); }
	[[nodiscard]] const Array&  array()   const{ return get<Array>("array"); }
	[[nodiscard]] String&       string()       { return get<String>("string"); }
	[[nodiscard]] Object&       object()       { return get<Object>("object"); }
	[[nodiscard]] Array&        array()        { return get<Array>("array"); }

	[[nodiscard]] Decimal number() const;

	[[nodiscard]] bool operator==(const Value& other) const = default;
	[[nodiscard]] bool operator!=(const Value& other) const = default;

	[[nodiscard]] const VariantType& variant() const{ return m_variant; }
	[[nodiscard]] VariantType& variant(){ return m_variant; }

private:
	VariantType m_variant;

	template<typename T>
	T& get(const char* typeName);

	template<typename T>
	const T& get(const char* typeName) const;

	[[noreturn]] static void throwTypeError(const char* expectedType);
};

} // namespace lsp::json

#include "json.inl"
