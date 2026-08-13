#include "json.h"

namespace lsp::json{
namespace{

} // namespace

Object::Object()
	: m_map{std::make_unique<MapType>()}
{
}

Object::Object(const Object& other)
	: m_map{std::make_unique<MapType>(*other.m_map)}
{
}

Object::~Object() = default;

Object& Object::operator=(const Object& other)
{
	*this->m_map = *other.m_map;
	return *this;
}

bool Object::operator==(const Object& other) const
{
	return *this->m_map == *other.m_map;
}

std::size_t Object::size() const
{
	return m_map->size();
}

bool Object::empty() const
{
	return m_map->empty();
}

bool Object::contains(std::string_view key) const
{
	return m_map->contains(key);
}

Value& Object::operator[](std::string_view key)
{
	if(const auto it = m_map->find(key); it != m_map->end())
		return it->second;

	return m_map->insert({std::string(key), Value()}).first->second;
}

Value& Object::get(std::string_view key)
{
	if(const auto it = m_map->find(key); it != m_map->end())
		return it->second;

	throw TypeError("Missing key '" + std::string{key} + '\'');
}

const Value& Object::get(std::string_view key) const
{
	if(const auto it = m_map->find(key); it != m_map->end())
		return it->second;

	throw TypeError("Missing key '" + std::string{key} + '\'');
}

Value* Object::find(std::string_view key)
{
	if(const auto it = m_map->find(key); it != m_map->end())
		return &it->second;

	return nullptr;
}

const Value* Object::find(std::string_view key) const
{
	if(const auto it = m_map->find(key); it != m_map->end())
		return &it->second;

	return nullptr;
}

Object::MapType& Object::keyValueMap()
{
	return *m_map;
}

const Object::MapType& Object::keyValueMap() const
{
	return *m_map;
}

} // namespace lsp::json
