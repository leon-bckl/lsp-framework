#pragma once

#include <tuple>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <cstddef>
#include <variant>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <lsp/util/util.h>

namespace lsp::json{

/*
 * Types
 */

class Any;

using Null    = std::nullptr_t;
using Boolean = bool;
using Decimal = double;
using Integer = long long;
using String  = std::string;
using Array   = std::vector<Any>;

class TypeError : public std::bad_variant_access{
public:
	TypeError() = default;
	TypeError(std::string message) : m_message{std::move(message)}{}

private:
	std::string m_message{"Unexpected json structure"};
};

using ObjectMap = std::unordered_map<String, Any>;
class Object : public ObjectMap{
public:
	using ObjectMap::ObjectMap;

	Any& get(const std::string& key);
	const Any& get(const std::string& key) const;

	template<typename T>
	T& get(const std::string& key);

	template<typename T>
	const T& get(const std::string& key) const;
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

	template<typename T>
	T& get(){
		try{
			return std::get<T>(*this);
		}catch(const std::bad_variant_access&){
			throw TypeError{};
		}
	}

	template<>
	Any& get(){
		return *this;
	}

	template<typename T>
	const T& get() const{
		try{
			return std::get<T>(*this);
		}catch(const std::bad_variant_access&){
			throw TypeError{};
		}
	}

	template<>
	const Any& get() const{
		return *this;
	}

	Decimal numberValue() const{
		if(isDecimal())
			return get<Decimal>();

		if(isInteger())
			return static_cast<Decimal>(get<Integer>());

		throw TypeError{};
	}
};

template<typename T>
inline T& Object::get(const std::string& key){
	return get(key).get<T>();
}

template<typename T>
inline const T& Object::get(const std::string& key) const{
	return get(key).get<T>();
}

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
std::string stringify(const Any& json);

} // namespace lsp::json
