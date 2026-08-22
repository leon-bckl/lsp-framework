#pragma once

#include <tuple>
#include <type_traits>
#include "json.h"

namespace lsp::json{

/*
 * Value
 */

template<typename T>
T& Value::get(const char* typeName)
{
	if(auto* const v = std::get_if<T>(&m_variant))
		return *v;

	throwTypeError(typeName);
}

template<typename T>
const T& Value::get(const char* typeName) const
{
	if(auto* const v = std::get_if<T>(&m_variant))
		return *v;

	throwTypeError(typeName);
}

/*
 * Object
 */

class Object::KeyValuePair{
public:
	KeyValuePair(String key, Value value)
		: m_key{std::move(key)}
		, m_value{std::move(value)}
	{
	}

	const String& key()   const{ return m_key; }
	Value&        value()      { return m_value; }
	const Value&  value() const{ return m_value; }

	template<std::size_t I>
	decltype(auto) get()
	{
		if constexpr(I == 0)
			return key();
		else
			return value();
	}

	template<std::size_t I>
	decltype(auto) get() const
	{
		if constexpr(I == 0)
			return key();
		else
			return value();
	}

private:
	String m_key;
	Value  m_value;
};

// Define inline functions now that KeyValuePair is complete

inline Object::Object() = default;
inline Object::Object(const Object&) = default;
inline Object::Object(Object&&) noexcept = default;
inline Object& Object::operator=(const Object&) = default;
inline Object& Object::operator=(Object&&) noexcept = default;
inline Object::~Object() = default;

inline auto Object::begin(){ return m_keyValuePairs.begin(); }
inline auto Object::begin() const{ return m_keyValuePairs.begin(); }
inline auto Object::cbegin() const{ return m_keyValuePairs.begin(); }
inline auto Object::end(){ return m_keyValuePairs.end(); }
inline auto Object::end() const{ return m_keyValuePairs.end(); }
inline auto Object::cend() const{ return m_keyValuePairs.cend(); }

} // namespace lsp::json

// tuple_size and tuple_element specializations for KeyValuePair to make it work with structued bindings
namespace std{

template<>
struct tuple_size<lsp::json::Object::KeyValuePair> : integral_constant<size_t, 2>{};

template<>
struct tuple_element<0, lsp::json::Object::KeyValuePair>{
	using type = const lsp::json::String;
};

template<>
struct tuple_element<1, lsp::json::Object::KeyValuePair>{
	using type = lsp::json::Value;
};

} // namespace std
