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

namespace lsp{
namespace json{

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
		}catch(const std::bad_variant_access& e){
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
		}catch(const std::bad_variant_access& e){
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

} // namespace json

/*
 * To/From json
 */

template<typename T>
class Nullable{
public:
	Nullable() = default;

	Nullable& operator=(const T& t){
		m_value = t;
		return *this;
	}

	Nullable& operator=(std::nullptr_t){
		m_value = std::nullopt;
		return *this;
	}

	bool isNull() const{ return !m_value.has_value(); }
	T& value(){ return m_value.value(); }
	const T& value() const{ return m_value.value(); }

private:
	std::optional<T> m_value;
};

// toJson

template<typename T>
json::Any toJson(const T& v){
	return v;
}

template<typename... Args>
json::Any toJson(const std::tuple<Args...>& tuple){
	json::Array result;
	result.resize(sizeof...(Args));
	util::tuple::visit(tuple, [&result](const auto& v, int idx){
		result[idx] = toJson(v);
	});

	return result;
}

template<typename T>
json::Any toJson(const std::unordered_map<json::String, T>& map){
	json::Object result;
	for(const auto& [k, v] : map)
		result[k] = toJson(v);

	return result;
}

template<typename T>
json::Any toJson(const std::vector<T>& vector){
	json::Array result;
	result.reserve(vector.size());

	for(const auto& e : vector)
		result.push_back(toJson(e));

	return result;
}

template<typename... Args>
json::Any toJson(const std::variant<Args...>& variant){
	return std::visit([](auto& v){ return toJson(v); }, variant);
}

template<typename T>
json::Any toJson(const Nullable<T>& nullable){
	if(nullable.isNull())
		return nullptr;

	return toJson(nullable.value());
}

template<typename T>
json::Any toJson(const std::unique_ptr<T>& v){
	assert(v);
	return toJson(*v);
}

template<typename T>
json::Any toJson(const std::optional<T>& v){
	assert(v.has_value());
	return toJson(v.value());
}

template<>
inline json::Any toJson(const std::string_view& v){
	return json::String{v};
}

// fromJson

template<typename T>
void fromJson(const json::Any& json, T& value){
	value = json.get<T>();
}

template<typename... Args>
void fromJson(const json::Any& json, std::tuple<Args...>& value){
	const auto& array = json.get<json::Array>();
	util::tuple::visit(value, [&array](auto& v, int idx){
		if(static_cast<std::size_t>(idx) >= array.size())
			throw json::TypeError{};

		fromJson(array[idx], v);
	});
}

template<typename T>
void fromJson(const json::Any& json, std::unordered_map<json::String, T>& value){
	const auto& obj = json.get<json::Object>();
	for(const auto& [k, v] : obj)
		fromJson(v, value[k]);
}

template<typename T>
void fromJson(const json::Any& json, std::vector<T>& value){
	const auto& array = json.get<json::Array>();
	value.reserve(array.size());

	for(const auto& e : array)
		fromJson(e, value.emplace_back());
}

template<typename T>
const char** requiredProperties(){
	static const char* properties[] = {nullptr};
	return properties;
}

template<std::size_t Index, typename... Args, typename VariantType = std::variant<Args...>>
void variantFromJson(const json::Any& json, VariantType& value){
	using T = std::variant_alternative_t<Index, VariantType>;

	if constexpr(std::is_null_pointer_v<T>){
		if(json.isNull()){
			value.template emplace<Index>();
			return;
		}
	}

	if constexpr(std::is_same_v<T, bool>){
		if(json.isBoolean()){
			fromJson(json, value.template emplace<Index>());
			return;
		}
	}

	if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>){
		if(json.isNumber()){
			fromJson(json, value.template emplace<Index>());
			return;
		}
	}

	if constexpr(std::is_same_v<T, json::String>){
		if(json.isString()){
			fromJson(json, value.template emplace<Index>());
			return;
		}
	}

	if constexpr(util::type::IsVector<T>{} || util::type::IsTuple<T>{}){
		if(json.isArray()){
			fromJson(json, value.template emplace<Index>());
			return;
		}
	}

	if constexpr(std::is_class_v<T>){
		if(json.isObject()){
			const auto& obj = json.get<json::Object>();
			bool hasAllRequiredProperties = true;

			for(const char** p = requiredProperties<T>(); *p; ++p){
				if(!obj.contains(*p)){
					hasAllRequiredProperties = false;
					break;
				}
			}

			if(hasAllRequiredProperties){
				fromJson(json, value.template emplace<Index>());
				return;
			}
		}
	}

	if constexpr(Index + 1 < sizeof...(Args)){
		variantFromJson<Index + 1, Args...>(json, value);
	}else{
		throw json::TypeError{};
	}
}

template<typename... Args>
void fromJson(const json::Any& json, std::variant<Args...>& value){
	variantFromJson<0, Args...>(json, value);
}

template<typename T>
void fromJson(const json::Any& json, Nullable<T>& nullable){
	if(nullable.isNull())
		nullable = T{};

	fromJson(json, nullable.value());
}

template<typename T>
void fromJson(const json::Any& json, std::unique_ptr<T>& value){
	if(!value)
		value = std::make_unique<T>();

	fromJson(json, *value);
}

template<typename T>
void fromJson(const json::Any& json, std::optional<T>& value){
	if(!value.has_value())
		value = T{};

	fromJson(json, value.value());
}

template<>
inline void fromJson(const json::Any& json, int& value){
	value = static_cast<int>(json.numberValue());
}

template<>
inline void fromJson(const json::Any& json, unsigned int& value){
	value = static_cast<unsigned int>(json.numberValue());
}

template<>
inline void fromJson(const json::Any& json, long& value){
	value = static_cast<long>(json.numberValue());
}

template<>
inline void fromJson(const json::Any& json, unsigned long& value){
	value = static_cast<unsigned long>(json.numberValue());
}

template<>
inline void fromJson(const json::Any& json, unsigned long long& value){
	value = static_cast<unsigned long long>(json.numberValue());
}

template<>
inline void fromJson(const json::Any& json, float& value){
	value = static_cast<float>(json.numberValue());
}

} // namespace lsp
