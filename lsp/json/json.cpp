#include "json.h"

namespace lsp::json{
namespace{

} // namespace

/*
 * Value
 */

Decimal Value::number() const
{
	if(isDecimal())
		return get<Decimal>(nullptr);

	if(isInteger())
		return static_cast<Decimal>(get<Integer>(nullptr));

	throwTypeError("number");
}

void Value::throwTypeError(const char* expectedType)
{
	throw TypeError(std::string("JSON value is not ") + expectedType);
}

/*
 * Object
 */

Object::Object(std::initializer_list<KeyValuePair> pairs)
	: m_keyValuePairs{pairs}
{
	// TODO: Check for duplicates?
}

Object::SizeType Object::size() const
{
	return m_keyValuePairs.size();
}

Object::SizeType Object::capacity() const
{
	return m_keyValuePairs.capacity();
}

Value& Object::insert(String key, Value value)
{
	Value* existingValue = find(key);

	if(existingValue)
	{
		*existingValue = std::move(value);
		return *existingValue;
	}

	return m_keyValuePairs.emplace_back(std::move(key), std::move(value)).value();
}

void Object::remove(std::string_view key)
{
	for(auto it = m_keyValuePairs.begin(); it != m_keyValuePairs.end(); ++it)
	{
		if(it->key() == key)
		{
			m_keyValuePairs.erase(it);
			break;
		}
	}
}

void Object::clear()
{
	m_keyValuePairs.clear();
}

void Object::reserve(SizeType size)
{
	m_keyValuePairs.reserve(size);
}

Value* Object::find(std::string_view key)
{
	for(auto& [objKey, value] : m_keyValuePairs)
	{
		if(objKey == key)
			return &value;
	}

	return nullptr;
}

Value& Object::get(std::string_view key)
{
	auto* value = find(key);

	if(value)
		return *value;

	throw TypeError("JSON object does not contain key '" + std::string(key) + '\'');
}

Value& Object::operator[](std::string_view key)
{
	auto* value = find(key);

	if(value)
		return *value;

	return m_keyValuePairs.emplace_back(String(key), Value()).value();
}

bool Object::operator==(const Object& other) const
{
	if(size() != other.size())
		return false;

	for(const auto& [key, value] : m_keyValuePairs)
	{
		const auto* otherVal = other.find(key);

		if(!otherVal)
			return false;

		if(value != *otherVal)
			return false;
	}

	return true;
}

} // namespace lsp::json
