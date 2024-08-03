#pragma once

#include <tuple>
#include <limits>
#include <memory>
#include <cassert>
#include <iterator>
#include <algorithm>
#include <type_traits>
#include <lsp/str.h>
#include <lsp/util.h>
#include <lsp/fileuri.h>
#include <lsp/json/json.h>
#include <lsp/nullable.h>

namespace lsp{

// toJson

inline json::Any toJson(std::nullptr_t){ return {}; }
inline json::Any toJson(bool v){ return v; }
inline json::Any toJson(int i){ return i; }

inline json::Any toJson(unsigned int i)
{
	if(i <= static_cast<unsigned int>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Any toJson(long i)
{
	if(i <= static_cast<long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Any toJson(unsigned long i)
{
	if(i <= static_cast<unsigned long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Any toJson(long long i)
{
	if(i >= static_cast<long long>(std::numeric_limits<json::Integer>::min()) && i <= static_cast<long long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Any toJson(unsigned long long i)
{
	if(i <= static_cast<unsigned long long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Any toJson(float i){ return i; }
inline json::Any toJson(double i){ return i; }
inline json::Any toJson(std::string&& v){ return std::move(v); }
inline json::Any toJson(std::string_view v){ return json::String{v}; }
inline json::Any toJson(const FileURI& uri){ return uri.toString(); }
inline json::Any toJson(json::Any&& v){ return std::move(v); }
inline json::Any toJson(json::Object&& v){ return std::move(v); }
inline json::Any toJson(json::Array&& v){ return std::move(v); }

template<typename... Args>
json::Any toJson(std::tuple<Args...>&& tuple);

template<typename K, typename T>
json::Any toJson(str::UnorderedMap<K, T>&& map);

template<typename T>
json::Any toJson(std::vector<T>&& vector);

template<typename... Args>
json::Any toJson(std::variant<Args...>&& variant);

template<typename T>
json::Any toJson(Nullable<T>&& nullable);

template<typename... Args>
json::Any toJson(NullableVariant<Args...>&& nullable);

template<typename T>
json::Any toJson(std::unique_ptr<T>&& v);

template<typename T>
json::Any toJson(std::optional<T>&& v);

// impl

template<typename... Args>
json::Any toJson(std::tuple<Args...>&& tuple)
{
	json::Array result;
	result.resize(sizeof...(Args));
	util::tuple::visit(tuple, [&result](auto&& v, int idx)
	{
		result[idx] = toJson(std::forward<std::decay_t<decltype(v)>>(v));
	});

	return result;
}

namespace impl{

// Helpers to treat FileURI as a string which can be used to look up values in a json::Object

template<typename T>
struct MapKeyType{
	using Type = T;
};

template<>
struct MapKeyType<FileURI>{
	using Type = std::string;
};

template<typename T>
const typename MapKeyType<T>::Type& mapKey(const T& u)
{
	return u;
}

template<>
inline const std::string& mapKey(const FileURI& uri)
{
	return uri.path();
}

}

template<typename K, typename T>
json::Any toJson(str::UnorderedMap<K, T>&& map)
{
	json::Object result;
	for(auto&& [k, v] : map)
		result[impl::mapKey(k)] = toJson(std::move(v));

	return result;
}

template<typename T>
json::Any toJson(std::vector<T>&& vector)
{
	json::Array result;
	result.reserve(vector.size());
	std::transform(vector.begin(), vector.end(), std::back_inserter(result), [](auto&& e){ return toJson(std::forward<T>(e)); });
	return result;
}

template<typename... Args>
json::Any toJson(std::variant<Args...>&& variant)
{
	return std::visit([](auto&& v){ return toJson(std::forward<std::decay_t<decltype(v)>>(v)); }, variant);
}

template<typename T>
json::Any toJson(Nullable<T>&& nullable)
{
	if(nullable.isNull())
		return nullptr;

	return toJson(std::move(*nullable));
}

template<typename... Args>
json::Any toJson(NullableVariant<Args...>&& nullable)
{
	if(nullable.isNull())
		return nullptr;

	return toJson(std::move(*nullable));
}

template<typename T>
json::Any toJson(std::unique_ptr<T>&& v)
{
	assert(v);
	return toJson(std::move(*v));
}

template<typename T>
json::Any toJson(std::optional<T>&& v)
{
	assert(v.has_value());
	return toJson(std::move(*v));
}

// fromJson

inline void fromJson(json::Any&&, std::nullptr_t){}
inline void fromJson(json::Any&& json, bool& value){ value = json.boolean(); }
inline void fromJson(json::Any&& json, int& value){ value = static_cast<int>(json.number()); }
inline void fromJson(json::Any&& json, unsigned int& value){ value = static_cast<unsigned int>(json.number()); }
inline void fromJson(json::Any&& json, long& value){ value = static_cast<long>(json.number()); }
inline void fromJson(json::Any&& json, unsigned long& value){ value = static_cast<unsigned long>(json.number()); }
inline void fromJson(json::Any&& json, unsigned long long& value){ value = static_cast<unsigned long long>(json.number()); }
inline void fromJson(json::Any&& json, float& value){ value = static_cast<float>(json.number()); }
inline void fromJson(json::Any&& json, double& value){ value = static_cast<double>(json.number()); }
inline void fromJson(json::Any&& json, std::string& value){ value = std::move(json.string()); }
inline void fromJson(json::Any&& json, FileURI& value){ value = FileURI{json.string()}; }
inline void fromJson(json::Any&& json, json::Any& v){ v = std::move(json); }
inline void fromJson(json::Any&& json, json::Object& v){ v = std::move(json.object()); }
inline void fromJson(json::Any&& json, json::Array& v){ v = std::move(json.array()); }

template<typename... Args>
void fromJson(json::Any&& json, std::tuple<Args...>& value);

template<typename K, typename T>
void fromJson(json::Any&& json, str::UnorderedMap<K, T>& value);

template<typename T>
void fromJson(json::Any&& json, std::vector<T>& value);

template<typename... Args>
void fromJson(json::Any&& json, std::variant<Args...>& value);

template<typename... Args>
void fromJson(json::Any&& json, NullableVariant<Args...>& nullable);

template<typename T>
void fromJson(json::Any&& json, std::unique_ptr<T>& value);

template<typename T>
void fromJson(json::Any&& json, std::optional<T>& value);

// impl

template<typename... Args>
void fromJson(json::Any&& json, std::tuple<Args...>& value)
{
	auto& array = json.array();
	util::tuple::visit(value, [&array](auto& v, int idx)
	{
		if(static_cast<std::size_t>(idx) >= array.size())
			throw json::TypeError{};

		fromJson(std::move(array[idx]), v);
	});
}

template<typename K, typename T>
void fromJson(json::Any&& json, str::UnorderedMap<K, T>& value)
{
	auto& obj = json.object();
	for(auto&& [k, v] : obj)
		fromJson(std::move(v), value[k]);
}

template<typename T>
void fromJson(json::Any&& json, std::vector<T>& value)
{
	auto& array = json.array();
	value.reserve(array.size());

	for(auto&& e : array)
		fromJson(std::move(e), value.emplace_back());
}

template<typename T>
const char** requiredProperties()
{
	static const char* properties[] = {nullptr};
	return properties;
}

template<std::size_t Index, typename... Args, typename VariantType = std::variant<Args...>>
void variantFromJson(json::Any&& json, VariantType& value, int requiredPropertyCount = 0)
{
	using T = std::variant_alternative_t<Index, VariantType>;

	if constexpr(std::is_null_pointer_v<T>)
	{
		if(json.isNull())
		{
			value.template emplace<Index>();
			return;
		}
	}

	if constexpr(std::is_same_v<T, bool>)
	{
		if(json.isBoolean())
		{
			fromJson(std::move(json), value.template emplace<Index>());
			return;
		}
	}

	if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>)
	{
		if(json.isNumber())
		{
			fromJson(std::move(json), value.template emplace<Index>());
			return;
		}
	}

	if constexpr(std::is_same_v<T, std::string>)
	{
		if(json.isString())
		{
			fromJson(std::move(json), value.template emplace<Index>());
			return;
		}
	}

	if constexpr(util::type::IsVector<T>{} || util::type::IsTuple<T>{})
	{
		if(json.isArray())
		{
			fromJson(std::move(json), value.template emplace<Index>());
			return;
		}
	}

	// The object with the most required properties that match the json is constructed.
	// To achieve this, all alternatives are checked but only constructed if the number of matching properties is higher than it has been so far.
	if constexpr(std::is_class_v<T>)
	{
		if(json.isObject())
		{
			const auto& obj = json.object();
			bool hasAllRequiredProperties = true;
			int matchCount = 1; // Make this at least one to signify that an object was successfully created and no error is thrown at the end

			for(const char** p = requiredProperties<T>(); *p; ++p)
			{
				if(!obj.contains(*p))
				{
					hasAllRequiredProperties = false;
					break;
				}

				++matchCount;
			}

			if(hasAllRequiredProperties && matchCount >= requiredPropertyCount)
			{
				fromJson(std::move(json), value.template emplace<Index>());
				requiredPropertyCount = matchCount;
			}
		}
	}

	if constexpr(Index + 1 < sizeof...(Args))
	{
		variantFromJson<Index + 1, Args...>(std::move(json), value, requiredPropertyCount);
	}
	else if(requiredPropertyCount == 0)
	{
		throw json::TypeError{};
	}
}

template<typename... Args>
void fromJson(json::Any&& json, std::variant<Args...>& value)
{
	variantFromJson<0, Args...>(std::move(json), value);
}

template<typename T>
void fromJson(json::Any&& json, Nullable<T>& nullable)
{
	if(json.isNull())
	{
		nullable.reset();
	}
	else
	{
		if(nullable.isNull())
			nullable = T{};

		fromJson(std::move(json), *nullable);
	}
}

template<typename... Args>
void fromJson(json::Any&& json, NullableVariant<Args...>& nullable)
{
	if(json.isNull())
	{
		nullable.reset();
	}
	else
	{
		if(nullable.isNull())
			nullable = typename NullableVariant<Args...>::VariantType{};

		fromJson(std::move(json), *nullable);
	}
}

template<typename T>
void fromJson(json::Any&& json, std::unique_ptr<T>& value)
{
	if(!value)
		value = std::make_unique<T>();

	fromJson(std::move(json), *value);
}

template<typename T>
void fromJson(json::Any&& json, std::optional<T>& value)
{
	if(!value.has_value())
		value = T{};

	fromJson(std::move(json), *value);
}

} // namespace lsp
