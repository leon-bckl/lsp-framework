#pragma once

#include <optional>
#include <cassert>
#include <stdexcept>
#include "json.h"

namespace lsp::json{

class TypeError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

template<typename T>
class Nullable{
public:
	Nullable() = default;
	Nullable(const T& t) : m_value{t}{}

	Nullable& operator=(const T& t){
		m_value = t;
		return *this;
	}

	Nullable& operator=(Null) noexcept{
		reset();
		return *this;
	}

	bool isNull() const{
		return !m_value.has_value();
	}

	T& value(){
		return m_value.value();
	}

	const T& value() const{
		return m_value.value();
	}

	void reset() noexcept{
		m_value.reset();
	}

private:
	std::optional<T> m_value;
};

template<typename T>
using Optional = std::optional<T>;

template<typename T>
using OptionalNullable = Optional<Nullable<T>>;

#define JSON_OBJ(className) \
private: \
	using ThisClass = className; \
	using FromJsonMethodPtr = void(ThisClass::*)(const ::lsp::json::Value&); \
	using ToJsonMethodPtr = ::lsp::json::Optional<::lsp::json::Value>(ThisClass::*)() const; \
	struct Property{ \
		bool optional; \
		FromJsonMethodPtr fromJson; \
		ToJsonMethodPtr toJson; \
	}; \
	static std::unordered_map<std::string, Property>& propertyMap(){ \
		static std::unordered_map<std::string, Property> map; \
		return map; \
	} \
public: \
	void fromJson(const ::lsp::json::Object& json){ \
		for(const auto& p : propertyMap()){ \
			auto it = json.find(p.first); \
			if(it != json.end()) \
				(this->*p.second.fromJson)(it->second); \
			else if(!p.second.optional) \
				throw ::lsp::json::TypeError{"Missing required property: " + p.first}; \
		} \
	} \
	::lsp::json::Object toJson() const{ \
		::lsp::json::Object json; \
		for(auto it = propertyMap().cbegin(); it != propertyMap().cend(); ++it){ \
			auto val = (this->*it->second.toJson)(); \
			if(val.has_value()) \
				json[it->first] = val.value(); \
		} \
		return json;\
	}

#define JSON_PROPERTY(type, name, ...) \
public: \
	type name{__VA_ARGS__}; \
private: \
	void name ## FromJson(const ::lsp::json::Value& json){ \
		::lsp::json::impl::fromJson(name, json); \
	} \
	::lsp::json::Optional<::lsp::json::Value> name ## ToJson() const{ \
		return ::lsp::json::impl::hasValue(name) ? ::lsp::json::Optional<::lsp::json::Value>{::lsp::json::impl::toJson(::lsp::json::impl::value(name))} : std::nullopt; \
	} \
	inline static const bool _ ## name ## JsonImpl = propertyMap().insert({#name, Property{::lsp::json::impl::IsOptional<decltype(name)>::value, &ThisClass::name ## FromJson, &ThisClass::name ## ToJson}}).second; \
public:

namespace impl{

/*
 * To JSON
 */

template<typename T>
struct IsOptional{
	static constexpr bool value = false;
};

template<typename T>
struct IsOptional<Optional<T>>{
	static constexpr bool value = true;
};

template<typename T>
inline bool hasValue(const T&){
	return true;
}

template<typename T>
inline bool hasValue(const Optional<T>& o){
	return o.has_value();
}

template<typename T>
inline const T& value(const T& t){
	return t;
}

template<typename T>
inline const T& value(const Optional<T>& o){
	return o.value();
}

template<typename T>
inline Value toJson(const T& t){
	return t.toJson();
}

template<typename T>
inline Value toJson(const std::vector<T>& t){
	Array array;
	array.resize(t.size());

	for(std::size_t i = 0; i < array.size(); ++i)
		array[i] = toJson(t[i]);

	return array;
}

template<typename T>
inline Value toJson(const Nullable<T>& n){
	return n.isNull() ? Value{} : toJson(n.value());
}

template<typename T>
inline Value toJson(const Optional<T>&){
	assert(!"toJson should never be called for Optional");
	std::abort();
	return {};
}

template<>
inline Value toJson(const bool& b){ return b; }

template<>
inline Value toJson(const int& i){ return static_cast<Number>(i); }

template<>
inline Value toJson(const unsigned int& i){ return static_cast<Number>(i); }

template<>
inline Value toJson(const long& i){ return static_cast<Number>(i); }

template<>
inline Value toJson(const unsigned long& i){ return static_cast<Number>(i); }

template<>
inline Value toJson(const float& f){ return static_cast<Number>(f); }

template<>
inline Value toJson(const double& d){ return d; }

template<>
inline Value toJson(const std::string& s){ return s; }

template<>
inline Value toJson(const Value& v){ return v; }

/*
 * From JSON
 */

template<typename T>
inline void checkType(const Value& json){
	if(json.isNull())
		throw TypeError{"Null"};

	if(!std::holds_alternative<T>(json))
		throw TypeError{"Type"};
}

template<typename T>
inline void fromJson(T& t, const Value& json){
	checkType<Object>(json);
	t.fromJson(std::get<Object>(json));
}

template<typename T>
inline void fromJson(std::vector<T>& t, const Value& json){
	checkType<Array>(json);
	const auto& array = std::get<Array>(json);
	t.resize(array.size());
	for(std::size_t i = 0; i < t.size(); ++i)
		fromJson(t[i], array[i]);
}

template<typename T>
inline void fromJson(Nullable<T>& n, const Value& json){
	if(json.isNull()){
		n.reset();
	}else{
		n = T{};
		fromJson(n.value(), json);
	}
}

template<typename T>
inline void fromJson(Optional<T>& o, const Value& json){
	o = T{};
	fromJson(o.value(), json);
}

template<>
inline void fromJson(bool& b, const Value& json){
	checkType<bool>(json);
	b = std::get<bool>(json);
}

template<>
inline void fromJson(int& i, const Value& json){
	checkType<Number>(json);
	i = static_cast<int>(std::get<Number>(json));
}

template<>
inline void fromJson(unsigned int& i, const Value& json){
	checkType<Number>(json);
	i = static_cast<unsigned int>(std::get<Number>(json));
}

template<>
inline void fromJson(long& l, const Value& json){
	checkType<Number>(json);
	l = static_cast<long>(std::get<Number>(json));
}

template<>
inline void fromJson(unsigned long& l, const Value& json){
	checkType<Number>(json);
	l = static_cast<unsigned long>(std::get<Number>(json));
}

template<>
inline void fromJson(float& f, const Value& json){
	checkType<Number>(json);
	f = static_cast<float>(std::get<Number>(json));
}

template<>
inline void fromJson(double& d, const Value& json){
	checkType<double>(json);
	d = std::get<double>(json);
}

template<>
inline void fromJson(std::string& s, const Value& json){
	checkType<std::string>(json);
	s = std::get<std::string>(json);
}

template<>
inline void fromJson(Value& v, const Value& json){
	v = json;
}

}

}
