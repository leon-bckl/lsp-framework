#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
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
	TypeError(const std::string& message = "Unexpected json value")
		: Error(message)
		{
		}
};

class ParseError : public Error{
public:
	ParseError(const std::string& message, std::size_t textPos)
		: Error(message)
		, m_textPos(textPos)
	{
	}

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

auto parse(std::string_view text) -> Value;
auto stringify(const Value& json, std::string_view indent = {}) -> std::string;

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

	[[nodiscard]] auto size() const -> SizeType;
	[[nodiscard]] auto capacity() const -> SizeType;
	[[nodiscard]] auto isEmpty() const -> bool{ return size() == 0; }

	auto insert(String key, Value value) -> Value&;
	auto append(String key, Value value) -> Value&;
	void remove(std::string_view key);
	void clear();
	void reserve(SizeType size);

	[[nodiscard]] auto find(std::string_view key) -> Value*;
	[[nodiscard]] auto find(std::string_view key) const -> const Value*{ return const_cast<Object*>(this)->find(key); }
	[[nodiscard]] auto contains(std::string_view key) const -> bool{ return find(key) != nullptr; }
	[[nodiscard]] auto get(std::string_view key) -> Value&;
	[[nodiscard]] auto get(std::string_view key) const -> const Value&{ return const_cast<Object*>(this)->get(key); }

	[[nodiscard]] auto operator[](std::string_view key) -> Value&;

	[[nodiscard]] auto operator==(const Object& other) const -> bool;
	[[nodiscard]] auto operator!=(const Object& other) const -> bool{ return !(*this == other); }

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

	// Template constructors to prevent accidential conversions (e.g., pointer to bool)
	// without having to make constructors explicit which is inconvenient for this class.

	template<std::same_as<Boolean> T>
	constexpr Value(T b) : m_variant(b){}

	template<typename T>
	requires (std::integral<T> && !std::same_as<T, Boolean>)
	constexpr Value(T i)
	{
		using Common = std::common_type_t<T, Integer>;

		if(static_cast<Common>(i) > static_cast<Common>(std::numeric_limits<Integer>::max()))
		{
			m_variant = static_cast<Decimal>(i);
			return;
		}

		if constexpr(std::unsigned_integral<T>)
		{
			if(static_cast<Common>(i) < static_cast<Common>(std::numeric_limits<Integer>::min()))
			{
				m_variant = static_cast<Decimal>(i);
				return;
			}
		}

		m_variant = static_cast<Integer>(i);
	}

	template<std::floating_point T>
	constexpr Value(T d) : m_variant(static_cast<Decimal>(d)){}

	Value(const char* s)      : m_variant(String(s)){}
	Value(std::string_view s) : m_variant(String(s)){}
	Value(const String& s)    : m_variant(s){}
	Value(String&& s)         : m_variant(std::move(s)){}
	Value(const Array& a)     : m_variant(a){}
	Value(Array&& a)          : m_variant(std::move(a)){}
	Value(const Object& o)    : m_variant(o){}
	Value(Object&& o)         : m_variant(std::move(o)){}

	template<typename T>
	Value(T) = delete;

	[[nodiscard]] constexpr auto isNull()    const -> bool{ return std::holds_alternative<Null>(m_variant); }
	[[nodiscard]] constexpr auto isBoolean() const -> bool{ return std::holds_alternative<Boolean>(m_variant); }
	[[nodiscard]] constexpr auto isInteger() const -> bool{ return std::holds_alternative<Integer>(m_variant); }
	[[nodiscard]] constexpr auto isDecimal() const -> bool{ return std::holds_alternative<Decimal>(m_variant); }
	[[nodiscard]] constexpr auto isNumber()  const -> bool{ return isInteger() || isDecimal(); }
	[[nodiscard]] constexpr auto isString()  const -> bool{ return std::holds_alternative<String>(m_variant); }
	[[nodiscard]] constexpr auto isObject()  const -> bool{ return std::holds_alternative<Object>(m_variant); }
	[[nodiscard]] constexpr auto isArray()   const -> bool{ return std::holds_alternative<Array>(m_variant); }

	[[nodiscard]] auto boolean() const -> Boolean{ return get<Boolean>("boolean"); }
	[[nodiscard]] auto integer() const -> Integer{ return get<Integer>("integer"); }
	[[nodiscard]] auto decimal() const -> Decimal{ return get<Decimal>("decimal"); }
	[[nodiscard]] auto string() const -> const String&{ return get<String>("string"); }
	[[nodiscard]] auto object() const -> const Object&{ return get<Object>("object"); }
	[[nodiscard]] auto array() const -> const Array&{ return get<Array>("array"); }
	[[nodiscard]] auto string() -> String&{ return get<String>("string"); }
	[[nodiscard]] auto object() -> Object&{ return get<Object>("object"); }
	[[nodiscard]] auto array() -> Array&{ return get<Array>("array"); }

	[[nodiscard]] auto number() const -> Decimal;

	[[nodiscard]] auto operator==(const Value& other) const -> bool = default;
	[[nodiscard]] auto operator!=(const Value& other) const -> bool = default;

	[[nodiscard]] auto variant() const -> const VariantType&{ return m_variant; }
	[[nodiscard]] auto variant() -> VariantType&{ return m_variant; }

private:
	VariantType m_variant;

	template<typename T>
	auto get(const char* typeName) -> T&;

	template<typename T>
	auto get(const char* typeName) const -> const T&;

	[[noreturn]] static void throwTypeError(const char* expectedType);
};

} // namespace lsp::json

#include "json.inl"
