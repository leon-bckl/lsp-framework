#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <lsp/enumeration.h>
#include <lsp/json/json.h>
#include <lsp/json/writer.h>
#include <lsp/nullable.h>
#include <lsp/uri.h>

namespace lsp{

/*
 * toJson
 */

template<typename T, json::SimpleWriter WriterType>
void toJson(const T& value, WriterType& writer);
template<typename T>
void toJson(std::string_view key, const T& value, json::ObjectWriter& objectWriter);

// std::vector toJson

template<typename T, json::SimpleWriter WriterType>
void toJson(const std::vector<T>& values, WriterType& writer);
template<typename T>
void toJson(std::string_view key, const std::vector<T>& values, json::ObjectWriter& objectWriter);

// std::tuple toJson

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const std::tuple<Args...>& tuple, WriterType& writer);
template<typename... Args>
void toJson(std::string_view key, const std::tuple<Args...>& tuple, json::ObjectWriter& objectWriter);

// std::unordered_map toJson

template<typename K, typename V, json::SimpleWriter WriterType>
void toJson(const std::unordered_map<K, V>& values, WriterType& writer);
template<typename K, typename V>
void toJson(std::string_view key, const std::unordered_map<K, V>& values, json::ObjectWriter& objectWriter);

// std::variant toJson

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const std::variant<Args...>& variant, WriterType& writer);
template<typename... Args>
void toJson(std::string_view key, const std::variant<Args...>& variant, json::ObjectWriter& objectWriter);

// std::optional toJson

template<typename T, json::SimpleWriter WriterType>
void toJson(const std::optional<T>& opt, WriterType& writer);
template<typename T>
void toJson(std::string_view key, const std::optional<T>& opt, json::ObjectWriter& objectWriter);

// std::unique_ptr toJson

template<typename T, json::SimpleWriter WriterType>
void toJson(const std::unique_ptr<T>& opt, WriterType& writer);
template<typename T>
void toJson(std::string_view key, const std::unique_ptr<T>& opt, json::ObjectWriter& objectWriter);

// Nullable toJson

template<typename T, json::SimpleWriter WriterType>
void toJson(const Nullable<T>& nullable, WriterType& writer);
template<typename T>
void toJson(std::string_view key, const Nullable<T>& nullable, json::ObjectWriter& objectWriter);

// NullableVariant toJson

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const NullableVariant<Args...>& nullable, WriterType& writer);
template<typename... Args>
void toJson(std::string_view key, const NullableVariant<Args...>& nullable, json::ObjectWriter& objectWriter);

// Enumeration toJson

template<typename E, typename T, json::SimpleWriter WriterType>
void toJson(const Enumeration<E, T>& enumeration, WriterType& writer);
template<typename E, typename T>
void toJson(std::string_view key, const Enumeration<E, T>& enumeration, json::ObjectWriter& objectWriter);

// Uri toJson

void toJson(const Uri& uri, json::Writer& writer);
void toJson(const Uri& uri, json::ArrayWriter& arrayWriter);
void toJson(std::string_view key, const Uri& uri, json::ObjectWriter& objectWriter);


// TODO: Remoe old toJson functions


inline json::Value toJson(std::nullptr_t){ return {}; }
inline json::Value toJson(bool v){ return v; }
inline json::Value toJson(int i){ return i; }
inline json::Value toJson(float i){ return i; }
inline json::Value toJson(double i){ return i; }
inline json::Value toJson(std::string&& v){ return std::move(v); }
inline json::Value toJson(const std::string& v){ return json::String{v}; }
inline json::Value toJson(std::string_view v){ return json::String{v}; }
inline json::Value toJson(const Uri& uri){ return uri.toString(); }
inline json::Value toJson(json::Value&& v){ return std::move(v); }
inline json::Value toJson(json::Object&& v){ return std::move(v); }
inline json::Value toJson(json::Array&& v){ return std::move(v); }

template<typename... Args>
json::Value toJson(std::tuple<Args...>&& tuple);

template<typename K, typename T>
json::Value toJson(std::unordered_map<K, T>&& map);

template<typename T>
json::Value toJson(std::vector<T>&& vector);

template<typename... Args>
json::Value toJson(std::variant<Args...>&& variant);

template<typename EnumType, typename ValueType>
json::Value toJson(Enumeration<EnumType, ValueType>&& enumeration);

template<typename T>
json::Value toJson(Nullable<T>&& nullable);

template<typename... Args>
json::Value toJson(NullableVariant<Args...>&& nullable);

template<typename T>
json::Value toJson(std::unique_ptr<T>&& v);

template<typename T>
json::Value toJson(std::optional<T>&& v);

// fromJson

template<typename T>
const std::pair<const char*, json::Value>* literalProperties()
{
	static std::pair<const char*, json::Value> properties[] = {{nullptr, {}}};
	return properties;
}

template<typename T>
const char** requiredProperties()
{
	static const char* properties[] = {nullptr};
	return properties;
}

inline void fromJson(json::Value&&, std::nullptr_t){}
inline void fromJson(json::Value&& json, bool& value){ value = json.boolean(); }
inline void fromJson(json::Value&& json, int& value){ value = static_cast<int>(json.number()); }
inline void fromJson(json::Value&& json, unsigned int& value){ value = static_cast<unsigned int>(json.number()); }
inline void fromJson(json::Value&& json, long& value){ value = static_cast<long>(json.number()); }
inline void fromJson(json::Value&& json, unsigned long& value){ value = static_cast<unsigned long>(json.number()); }
inline void fromJson(json::Value&& json, unsigned long long& value){ value = static_cast<unsigned long long>(json.number()); }
inline void fromJson(json::Value&& json, float& value){ value = static_cast<float>(json.number()); }
inline void fromJson(json::Value&& json, double& value){ value = static_cast<double>(json.number()); }
inline void fromJson(json::Value&& json, std::string& value){ value = std::move(json.string()); }
inline void fromJson(json::Value&& json, Uri& value){ value = Uri::parse(json.string()); }
inline void fromJson(json::Value&& json, json::Value& v){ v = std::move(json); }
inline void fromJson(json::Value&& json, json::Object& v){ v = std::move(json.object()); }
inline void fromJson(json::Value&& json, json::Array& v){ v = std::move(json.array()); }

template<typename... Args>
void fromJson(json::Value&& json, std::tuple<Args...>& value);

template<typename K, typename T>
void fromJson(json::Value&& json, std::unordered_map<K, T>& value);

template<typename T>
void fromJson(json::Value&& json, std::vector<T>& value);

template<typename... Args>
void fromJson(json::Value&& json, std::variant<Args...>& value);

template<typename EnumType, typename ValueType>
void fromJson(json::Value&& json, Enumeration<EnumType, ValueType>& enumeration);

template<typename T>
void fromJson(json::Value&& json, Nullable<T>& nullable);

template<typename... Args>
void fromJson(json::Value&& json, NullableVariant<Args...>& nullable);

template<typename T>
void fromJson(json::Value&& json, std::unique_ptr<T>& value);

template<typename T>
void fromJson(json::Value&& json, std::optional<T>& value);

namespace impl{

// Helpers to treat Uri as a string which can be used to look up values in a json::Object

template<typename T>
struct MapKeyType{
	using Type = const T&;
};

template<>
struct MapKeyType<Uri>{
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

template<typename T>
struct IsNullable : std::false_type{};

template<typename... Args>
struct IsNullable<Nullable<Args...>> : std::true_type{};

template<typename... Args>
struct IsNullable<NullableVariant<Args...>> : std::true_type{};

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
std::size_t deserializableVariantIndex(const json::Value& json);

template<std::size_t Index, typename TupleType>
bool canDeserializeTupleElementsFromJson(const json::Value& json);

template<std::size_t Index, typename TupleType>
bool canDeserializeTupleFromJson(const json::Array& array);

template<typename T>
bool canDeserializeTypeFromJson(const json::Value& json)
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
	else if constexpr(IsNullable<T>{})
	{
		return json.isNull() || canDeserializeTypeFromJson<typename T::value_type>(json);
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
			const auto& obj                  = json.object();
			bool        hasLiteralProperties = true;

			for(const auto* p = literalProperties<T>(); p->first; ++p)
			{
				if(const auto* val = obj.find(p->first); val != nullptr)
				{
					if(*val != p->second)
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
std::size_t deserializableVariantIndex(const json::Value& json)
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
void variantFromJson(json::Value&& json, VariantType& variant, const std::size_t idx)
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

/*
 * toJson
 */

template<typename T, json::SimpleWriter WriterType>
void toJson(const T& value, WriterType& writer)
{
	if constexpr(json::JsonPrimitive<T>)
	{
		writer.write(value);
	}
	else
	{
		auto objectWriter = writer.beginObject();
		toJson(value, objectWriter);
	}
}

template<typename T>
void toJson(std::string_view key, const T& value, json::ObjectWriter& objectWriter)
{
	if constexpr(json::JsonPrimitive<T>)
	{
		objectWriter.write(key, value);
	}
	else
	{
		auto nestedObjectWriter = objectWriter.beginObject(key);
		toJson(value, nestedObjectWriter);
	}
}

/*
 * std::vector toJson
 */

template<typename T, json::SimpleWriter WriterType>
void toJson(const std::vector<T>& values, WriterType& writer)
{
	auto arrayWriter = writer.beginArray();

	for(const auto& v : values)
		toJson(v, arrayWriter);
}

template<typename T>
void toJson(std::string_view key, const std::vector<T>& values, json::ObjectWriter& objectWriter)
{
	auto arrayWriter = objectWriter.beginArray(key);

	for(const auto& v : values)
		toJson(v, arrayWriter);
}

/*
 * std::tuple toJson
 */

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const std::tuple<Args...>& tuple, WriterType& writer)
{
	auto arrayWriter = writer.beginArray();

	std::apply([&arrayWriter](auto&&... tupleArgs){
		(toJson(tupleArgs, arrayWriter), ...);
	}, tuple);
}

template<typename... Args>
void toJson(std::string_view key, const std::tuple<Args...>& tuple, json::ObjectWriter& objectWriter)
{
	auto arrayWriter = objectWriter.beginArray(key);

	std::apply([&arrayWriter](auto&&... tupleArgs){
		(toJson(tupleArgs, arrayWriter), ...);
	}, tuple);
}

/*
 * std::unordered_map toJson
 */

template<typename K, typename V, json::SimpleWriter WriterType>
void toJson(const std::unordered_map<K, V>& values, WriterType& writer)
{
	auto objectWriter = writer.beginObject();

	for(const auto& [k, v] : values)
		toJson(impl::mapKey(k), v, objectWriter);
}

template<typename K, typename V>
void toJson(std::string_view key, const std::unordered_map<K, V>& values, json::ObjectWriter& objectWriter)
{
	auto nestedObjectWriter = objectWriter.beginObject(key);

	for(const auto& [k, v] : values)
		toJson(impl::mapKey(k), v, nestedObjectWriter);
}

/*
 * std::variant toJson
 */

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const std::variant<Args...>& variant, WriterType& writer)
{
	std::visit([&writer](const auto& v){ toJson(v, writer); }, variant);
}

template<typename... Args>
void toJson(std::string_view key, const std::variant<Args...>& variant, json::ObjectWriter& objectWriter)
{
	std::visit([key, &objectWriter](const auto& v){ toJson(key, v, objectWriter); }, variant);
}

/*
 * Nullable toJson
 */

template<typename T, json::SimpleWriter WriterType>
void toJson(const Nullable<T>& nullable, WriterType& writer)
{
	if(nullable.isNull())
		writer.write(nullptr);
	else
		toJson(nullable.value(), writer);
}

template<typename T>
void toJson(std::string_view key, const Nullable<T>& nullable, json::ObjectWriter& objectWriter)
{
	if(nullable.isNull())
		objectWriter.write(key, nullptr);
	else
		toJson(key, nullable.value(), objectWriter);
}

/*
 * NullableVariant toJson
 */

template<typename... Args, json::SimpleWriter WriterType>
void toJson(const NullableVariant<Args...>& nullable, WriterType& writer)
{
	if(nullable.isNull())
		writer.write(nullptr);
	else
		toJson(nullable.value(), writer);
}

template<typename... Args>
void toJson(std::string_view key, const NullableVariant<Args...>& nullable, json::ObjectWriter& objectWriter)
{
	if(nullable.isNull())
		objectWriter.write(key, nullptr);
	else
		toJson(key, nullable.value(), objectWriter);
}

/*
 * Enumeration toJson
 */

template<typename E, typename T, json::SimpleWriter WriterType>
void toJson(const Enumeration<E, T>& enumeration, WriterType& writer)
{
	toJson(enumeration.value(), writer);
}

template<typename E, typename T>
void toJson(std::string_view key, const Enumeration<E, T>& enumeration, json::ObjectWriter& objectWriter)
{
	toJson(key, enumeration.value(), objectWriter);
}

/*
 * Uri toJson
 */

inline void toJson(const Uri& uri, json::Writer& writer)
{
	writer.write(uri.toString());
}

inline void toJson(std::string_view key, const Uri& uri, json::ObjectWriter& objectWriter)
{
	objectWriter.write(key, uri.toString());
}

inline void toJson(const Uri& uri, json::ArrayWriter& arrayWriter)
{
	arrayWriter.write(uri.toString());
}

/*
 * std::optional toJson
 */

template<typename T>
void toJson(const std::optional<T>& opt, json::Writer& writer)
{
	if(opt.has_value())
		toJson(opt.value(), writer);
	else
		writer.write(nullptr);
}

template<typename T>
void toJson(const std::optional<T>& opt, json::ArrayWriter& arrayWriter)
{
	if(opt.has_value())
		toJson(opt.value(), arrayWriter);
	else
		arrayWriter.write(nullptr);
}

template<typename T>
void toJson(std::string_view key, const std::optional<T>& opt, json::ObjectWriter& objectWriter)
{
	if(opt.has_value())
		toJson(key, opt.value(), objectWriter);
	else
		objectWriter.write(key, nullptr);
}

/*
 * std::unique_ptr toJson
 */

template<typename T>
void toJson(const std::unique_ptr<T>& opt, json::Writer& writer)
{
	if(opt)
		toJson(*opt, writer);
	else
		writer.write(nullptr);
}

template<typename T>
void toJson(const std::unique_ptr<T>& opt, json::ArrayWriter& arrayWriter)
{
	if(opt)
		toJson(*opt, arrayWriter);
	else
		arrayWriter.write(nullptr);
}

template<typename T>
void toJson(std::string_view key, const std::unique_ptr<T>& opt, json::ObjectWriter& objectWriter)
{
	if(opt)
		toJson(key, *opt, objectWriter);
	else
		objectWriter.write(key, nullptr);
}

// TODO: Remove old toJson functions

inline json::Value toJson(unsigned int i)
{
	if(i <= static_cast<unsigned int>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Value toJson(long i)
{
	if(i <= static_cast<long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Value toJson(unsigned long i)
{
	if(i <= static_cast<unsigned long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Value toJson(long long i)
{
	if(i >= static_cast<long long>(std::numeric_limits<json::Integer>::min()) && i <= static_cast<long long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

inline json::Value toJson(unsigned long long i)
{
	if(i <= static_cast<unsigned long long>(std::numeric_limits<json::Integer>::max()))
		return static_cast<json::Integer>(i);

	return static_cast<json::Decimal>(i);
}

template<typename... Args>
json::Value toJson(std::tuple<Args...>&& tuple)
{
	json::Array result;
	result.reserve(sizeof...(Args));
	std::apply([&result](auto&&... tupleArgs){
		(result.push_back(toJson(std::move(tupleArgs))), ...);
	}, tuple);

	return result;
}

template<typename K, typename T>
json::Value toJson(std::unordered_map<K, T>&& map)
{
	json::Object result;
	for(auto&& [k, v] : map)
		result[impl::mapKey(k)] = toJson(std::move(v));

	return result;
}

template<typename T>
json::Value toJson(std::vector<T>&& vector)
{
	json::Array result;
	result.reserve(vector.size());
	std::transform(vector.begin(), vector.end(), std::back_inserter(result), [](auto&& v){ return toJson(std::move(v)); });
	return result;
}

template<typename... Args>
json::Value toJson(std::variant<Args...>&& variant)
{
	return std::visit([](auto&& v){ return toJson(std::move(v)); }, variant);
}

template<typename EnumType, typename ValueType>
json::Value toJson(Enumeration<EnumType, ValueType>&& enumeration)
{
	return toJson(enumeration.value());
}

template<typename T>
json::Value toJson(Nullable<T>&& nullable)
{
	if(nullable.isNull())
		return nullptr;

	return toJson(std::move(*nullable));
}

template<typename... Args>
json::Value toJson(NullableVariant<Args...>&& nullable)
{
	if(nullable.isNull())
		return nullptr;

	return toJson(std::move(*nullable));
}

template<typename T>
json::Value toJson(std::unique_ptr<T>&& v)
{
	assert(v);
	return toJson(std::move(*v));
}

template<typename T>
json::Value toJson(std::optional<T>&& v)
{
	assert(v.has_value());
	return toJson(std::move(*v));
}

// fromJson

template<typename... Args>
void fromJson(json::Value&& json, std::tuple<Args...>& value)
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
void fromJson(json::Value&& json, std::unordered_map<K, T>& value)
{
	auto& obj = json.object();
	value.reserve(obj.size());

	for(auto&& [k, v] : obj)
		fromJson(std::move(v), value[k]);
}

template<typename T>
void fromJson(json::Value&& json, std::unordered_map<Uri, T>& value)
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
void fromJson(json::Value&& json, std::vector<T>& value)
{
	auto& array = json.array();
	value.reserve(array.size());

	for(auto&& e : array)
		fromJson(std::move(e), value.emplace_back());
}

template<typename... Args>
void fromJson(json::Value&& json, std::variant<Args...>& value)
{
	const auto idx = impl::deserializableVariantIndex<0, std::variant<Args...>>(json);
	impl::variantFromJson<0>(std::move(json), value, idx);
}

template<typename EnumType, typename ValueType>
void fromJson(json::Value&& json, Enumeration<EnumType, ValueType>& enumeration)
{
	ValueType value{};
	fromJson(std::move(json), value);
	enumeration = std::move(value);
}

template<typename T>
void fromJson(json::Value&& json, Nullable<T>& nullable)
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
void fromJson(json::Value&& json, NullableVariant<Args...>& nullable)
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
void fromJson(json::Value&& json, std::unique_ptr<T>& value)
{
	// Be lenient and allow null for optional values.
	// If the optional holds a value that can be null it is still set.
	if(json.isNull() && !impl::canDeserializeTypeFromJson<T>(json))
	{
		value.reset();
		return;
	}

	if(!value)
		value = std::make_unique<T>();

	fromJson(std::move(json), *value);
}

template<typename T>
void fromJson(json::Value&& json, std::optional<T>& value)
{
	// Be lenient and allow null for optional values.
	// If the optional holds a value that can be null it is still set.
	if(json.isNull() && !impl::canDeserializeTypeFromJson<T>(json))
	{
		value.reset();
		return;
	}

	if(!value.has_value())
		value = T{};

	fromJson(std::move(json), *value);
}

} // namespace lsp
