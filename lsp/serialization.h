#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <lsp/enumeration.h>
#include <lsp/fileuri.h>
#include <lsp/json/json.h>
#include <lsp/nullable.h>
#include <lsp/strmap.h>
#include <lsp/uri.h>

namespace lsp{

// toJson

inline json::Any toJson(std::nullptr_t){ return {}; }
inline json::Any toJson(bool v){ return v; }
inline json::Any toJson(int i){ return i; }
inline json::Any toJson(float i){ return i; }
inline json::Any toJson(double i){ return i; }
inline json::Any toJson(std::string&& v){ return std::move(v); }
inline json::Any toJson(const std::string& v){ return json::String{v}; }
inline json::Any toJson(std::string_view v){ return json::String{v}; }
inline json::Any toJson(const Uri& uri){ return uri.toString(); }
inline json::Any toJson(const FileUri& uri){ return uri.toString(); }
inline json::Any toJson(json::Any&& v){ return std::move(v); }
inline json::Any toJson(json::Object&& v){ return std::move(v); }
inline json::Any toJson(json::Array&& v){ return std::move(v); }

template<typename... Args>
json::Any toJson(std::tuple<Args...>&& tuple);

template<typename K, typename T>
json::Any toJson(StrMap<K, T>&& map);

template<typename T>
json::Any toJson(std::vector<T>&& vector);

template<typename... Args>
json::Any toJson(std::variant<Args...>&& variant);

template<typename EnumType, typename ValueType>
json::Any toJson(Enumeration<EnumType, ValueType>&& enumeration);

template<typename T>
json::Any toJson(Nullable<T>&& nullable);

template<typename... Args>
json::Any toJson(NullableVariant<Args...>&& nullable);

template<typename T>
json::Any toJson(std::unique_ptr<T>&& v);

template<typename T>
json::Any toJson(std::optional<T>&& v);

// fromJson

template<typename T>
const std::pair<const char*, json::Any>* literalProperties()
{
	static std::pair<const char*, json::Any> properties[] = {{nullptr, {}}};
	return properties;
}

template<typename T>
const char** requiredProperties()
{
	static const char* properties[] = {nullptr};
	return properties;
}

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
inline void fromJson(json::Any&& json, Uri& value){ value = Uri::parse(json.string()); }
inline void fromJson(json::Any&& json, FileUri& value){ value = Uri::parse(json.string()); }
inline void fromJson(json::Any&& json, json::Any& v){ v = std::move(json); }
inline void fromJson(json::Any&& json, json::Object& v){ v = std::move(json.object()); }
inline void fromJson(json::Any&& json, json::Array& v){ v = std::move(json.array()); }

template<typename... Args>
void fromJson(json::Any&& json, std::tuple<Args...>& value);

template<typename K, typename T>
void fromJson(json::Any&& json, StrMap<K, T>& value);

template<typename T>
void fromJson(json::Any&& json, std::vector<T>& value);

template<typename... Args>
void fromJson(json::Any&& json, std::variant<Args...>& value);

template<typename EnumType, typename ValueType>
void fromJson(json::Any&& json, Enumeration<EnumType, ValueType>& enumeration);

template<typename T>
void fromJson(json::Any&& json, Nullable<T>& nullable);

template<typename... Args>
void fromJson(json::Any&& json, NullableVariant<Args...>& nullable);

template<typename T>
void fromJson(json::Any&& json, std::unique_ptr<T>& value);

template<typename T>
void fromJson(json::Any&& json, std::optional<T>& value);

namespace impl{

// Helpers to treat FileURI as a string which can be used to look up values in a json::Object

template<typename T>
struct MapKeyType{
	using Type = const T&;
};

template<>
struct MapKeyType<Uri>{
	using Type = std::string;
};

template<>
struct MapKeyType<FileUri>{
	using Type = std::string;
};

template<typename T>
typename MapKeyType<T>::Type mapKey(const T& u)
{
	return u;
}

template<>
inline std::string mapKey(const Uri& uri)
{
	return uri.toString();
}

template<>
inline std::string mapKey(const FileUri& uri)
{
	return uri.toString();
}

template<typename T>
struct IsVector : std::false_type{};

template<typename... Args>
struct IsVector<std::vector<Args...>> : std::true_type{};

template<typename T>
struct IsVariant : std::false_type{};

template<typename... Args>
struct IsVariant<std::variant<Args...>> : std::true_type{};

template<typename T>
struct IsTuple : std::false_type{};

template<typename... Args>
struct IsTuple<std::tuple<Args...>> : std::true_type{};

template<typename T>
struct IsEnumeration : std::false_type{};

template<typename... Args>
struct IsEnumeration<Enumeration<Args...>> : std::true_type{};

template<std::size_t Index, typename VariantType>
std::size_t deserializableVariantIndex(const json::Any& json);

template<std::size_t Index, typename TupleType>
bool canDeserializeTupleElementsFromJson(const json::Any& json);

template<std::size_t Index, typename TupleType>
bool canDeserializeTupleFromJson(const json::Array& array);

template<typename T>
bool canDeserializeTypeFromJson(const json::Any& json)
{
	if constexpr(std::is_null_pointer_v<T>)
	{
		return json.isNull();
	}
	else if constexpr(std::is_same_v<T, bool>)
	{
		return json.isBoolean();
	}
	else if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>)
	{
		return json.isNumber();
	}
	else if constexpr(std::is_same_v<T, std::string>)
	{
		return json.isString();
	}
	else if constexpr(IsVector<T>{})
	{
		if(json.isArray())
		{
			const auto& array = json.array();
			return array.empty() || canDeserializeTypeFromJson<typename T::value_type>(array[0]);
		}

		return false;
	}
	else if constexpr(IsTuple<T>{})
	{
		return json.isArray() && canDeserializeTupleFromJson<0, T>(json.array());
	}
	else if constexpr(IsEnumeration<T>{})
	{
		if constexpr(std::is_integral_v<typename T::ValueType>)
		{
			return json.isNumber();
		}
		else
		{
			static_assert(std::same_as<typename T::ValueType, std::string>,
				"Enumeration expected to either use integral or string type");
			return json.isString();
		}
	}
	else if constexpr(IsVariant<T>{})
	{
		return deserializableVariantIndex<0, T>(json) != std::variant_npos;
	}
	else
	{
		if(json.isObject())
		{
			const auto& obj = json.object();
			bool        hasLiteralProperties = true;

			for(const auto* p = literalProperties<T>(); p->first; ++p)
			{
				if(const auto it = obj.find(p->first); it != obj.end())
				{
					if(it->second != p->second)
					{
						hasLiteralProperties = false;
						break;
					}
				}
			}

			if(hasLiteralProperties)
			{
				bool hasRequiredProperties = true;

				for(const auto* p = requiredProperties<T>(); *p; ++p)
				{
					if(!obj.contains(*p))
					{
						hasRequiredProperties = false;
						break;
					}
				}

				return hasRequiredProperties;
			}
		}

		return false;
	}
}

template<std::size_t Index, typename TupleType>
bool canDeserializeTupleFromJson(const json::Array& array)
{
	if constexpr(Index == 0) // Only perform this check one time for the first element
	{
		if(array.size() != std::tuple_size_v<TupleType>)
			return false;
	}

	using T = std::tuple_element_t<Index, TupleType>;

	if(canDeserializeTypeFromJson<T>(array[Index]))
		return true;

	if constexpr(Index + 1 < std::tuple_size_v<TupleType>)
		return canDeserializeTupleFromJson<Index + 1, TupleType>(array);
	else
		return false;
}

template<std::size_t Index, typename VariantType>
std::size_t deserializableVariantIndex(const json::Any& json)
{
	using T = std::variant_alternative_t<Index, VariantType>;

	if(canDeserializeTypeFromJson<T>(json))
		return Index;

	if constexpr(Index + 1 < std::variant_size_v<VariantType>)
		return deserializableVariantIndex<Index + 1, VariantType>(json);
	else
		return std::variant_npos;
}

template<std::size_t Index, typename VariantType>
void variantFromJson(json::Any&& json, VariantType& variant, const std::size_t idx)
{
	if(Index == idx)
	{
		if(variant.index() != Index)
			variant.template emplace<Index>();

		fromJson(std::move(json), std::get<Index>(variant));
	}
	else
	{
		if constexpr(Index + 1 < std::variant_size_v<VariantType>)
			variantFromJson<Index + 1>(std::move(json), variant, idx);
		else
			throw json::TypeError("Json does not match any of the expected variant types");
	}
}

} // namespace impl

// toJson

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

template<typename... Args>
json::Any toJson(std::tuple<Args...>&& tuple)
{
	json::Array result;
	result.reserve(sizeof...(Args));
	std::apply([&result](auto&&... tupleArgs){
		(result.push_back(toJson(std::forward<std::decay_t<decltype(tupleArgs)>>(tupleArgs))), ...);
	}, tuple);

	return result;
}

template<typename K, typename T>
json::Any toJson(StrMap<K, T>&& map)
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

template<typename EnumType, typename ValueType>
json::Any toJson(Enumeration<EnumType, ValueType>&& enumeration)
{
	return toJson(enumeration.value());
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

template<typename... Args>
void fromJson(json::Any&& json, std::tuple<Args...>& value)
{
	auto& array = json.array();

	if(sizeof...(Args) != array.size())
		throw json::TypeError("Incorrect number of tuple elements");

	std::apply(
		[&array](auto&&... tupleArgs)
		{
			auto idx = 0u;
			(fromJson(std::move(array[idx++]), tupleArgs), ...);
		}, value);
}

template<typename K, typename T>
void fromJson(json::Any&& json, StrMap<K, T>& value)
{
	auto& obj = json.object();
	for(auto&& [k, v] : obj)
		fromJson(std::move(v), value[k]);
}

template<typename T>
void fromJson(json::Any&& json, StrMap<Uri, T>& value)
{
	auto& obj = json.object();
	value.reserve(obj.size());

	for(auto&& [k, v] : obj)
	{
		auto uri = Uri::parse(k);

		if(uri.isValid())
			fromJson(std::move(v), value[Uri::parse(k)]);
	}
}

template<typename T>
void fromJson(json::Any&& json, StrMap<FileUri, T>& value)
{
	auto& obj = json.object();
	value.reserve(obj.size());

	for(auto&& [k, v] : obj)
	{
		auto fileUri = FileUri(Uri::parse(k));

		if(fileUri.isValid())
			fromJson(std::move(v), value[std::move(fileUri)]);
	}
}

template<typename T>
void fromJson(json::Any&& json, std::vector<T>& value)
{
	auto& array = json.array();
	value.reserve(array.size());

	for(auto&& e : array)
		fromJson(std::move(e), value.emplace_back());
}

template<typename... Args>
void fromJson(json::Any&& json, std::variant<Args...>& value)
{
	const auto idx = impl::deserializableVariantIndex<0, std::variant<Args...>>(json);
	impl::variantFromJson<0>(std::move(json), value, idx);
}

template<typename EnumType, typename ValueType>
void fromJson(json::Any&& json, Enumeration<EnumType, ValueType>& enumeration)
{
	ValueType value{};
	fromJson(std::move(json), value);
	enumeration = std::move(value);
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
