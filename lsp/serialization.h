#pragma once

#include <tuple>
#include <memory>
#include <cassert>
#include <type_traits>
#include <lsp/util/str.h>
#include <lsp/util/uri.h>
#include <lsp/util/util.h>
#include <lsp/json/json.h>
#include <lsp/util/nullable.h>

namespace lsp{

// toJson

template<typename T>
json::Any toJson(const T& v);

template<typename... Args>
json::Any toJson(const std::tuple<Args...>& tuple);

template<typename K, typename T>
json::Any toJson(const util::str::UnorderedMap<K, T>& map);

template<typename T>
json::Any toJson(const std::vector<T>& vector);

template<typename... Args>
json::Any toJson(const std::variant<Args...>& variant);

template<typename T>
json::Any toJson(const util::Nullable<T>& nullable);

template<typename T>
json::Any toJson(const std::unique_ptr<T>& v);

template<typename T>
json::Any toJson(const std::optional<T>& v);

// impl

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

template<typename K, typename T>
json::Any toJson(const util::str::UnorderedMap<K, T>& map){
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
json::Any toJson(const util::Nullable<T>& nullable){
	if(nullable.isNull())
		return nullptr;

	return toJson(*nullable);
}

template<typename T>
json::Any toJson(const std::unique_ptr<T>& v){
	assert(v);
	return toJson(*v);
}

template<typename T>
json::Any toJson(const std::optional<T>& v){
	assert(v.has_value());
	return toJson(*v);
}

template<>
inline json::Any toJson(const std::string_view& v){
	return json::String{v};
}

template<>
inline json::Any toJson(const util::FileURI& uri){
	return uri.toString();
}

// fromJson

template<typename T>
void fromJson(const json::Any& json, T& value);

template<typename... Args>
void fromJson(const json::Any& json, std::tuple<Args...>& value);

template<typename K, typename T>
void fromJson(const json::Any& json, util::str::UnorderedMap<K, T>& value);

template<typename T>
void fromJson(const json::Any& json, std::vector<T>& value);

template<typename... Args>
void fromJson(const json::Any& json, std::variant<Args...>& value);

template<typename T>
void fromJson(const json::Any& json, util::Nullable<T>& nullable);

template<typename T>
void fromJson(const json::Any& json, std::unique_ptr<T>& value);

template<typename T>
void fromJson(const json::Any& json, std::optional<T>& value);

// impl

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

template<typename K, typename T>
void fromJson(const json::Any& json, util::str::UnorderedMap<K, T>& value){
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
void variantFromJson(const json::Any& json, VariantType& value, int requiredPropertyCount = 0){
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

	// The object with the most required properties that match the json is constructed.
	// To achieve this, all alternatives are checked but only constructed if the number of matching properties is higher than it has been so far.
	if constexpr(std::is_class_v<T>){
		if(json.isObject()){
			const auto& obj = json.get<json::Object>();
			bool hasAllRequiredProperties = true;
			int matchCount = 1; // Make this at least one to signify that an object was successfully created and no error is thrown at the end

			for(const char** p = requiredProperties<T>(); *p; ++p){
				if(!obj.contains(*p)){
					hasAllRequiredProperties = false;
					break;
				}

				++matchCount;
			}

			if(hasAllRequiredProperties && matchCount >= requiredPropertyCount){
				fromJson(json, value.template emplace<Index>());
				requiredPropertyCount = matchCount;
			}
		}
	}

	if constexpr(Index + 1 < sizeof...(Args)){
		variantFromJson<Index + 1, Args...>(json, value, requiredPropertyCount);
	}else if(requiredPropertyCount == 0){
		throw json::TypeError{};
	}
}

template<typename... Args>
void fromJson(const json::Any& json, std::variant<Args...>& value){
	variantFromJson<0, Args...>(json, value);
}

template<typename T>
void fromJson(const json::Any& json, util::Nullable<T>& nullable){
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

template<>
inline void fromJson(const json::Any& json, util::FileURI& value){
	value = json.get<json::String>();
}

}
