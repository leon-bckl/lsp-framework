#pragma once

#include <type_traits>
#include <vector>
#include <string>
#include <string_view>

namespace lsp::json{

class ObjectWriter;
class ArrayWriter;
class Value;
class Object;
using Array = std::vector<Value>;

/*
 * Writer
 */

class Writer{
	friend class ObjectWriter;
	friend class ArrayWriter;
	friend std::string stringify(const Value&, std::string_view); // Needed to write raw values
public:
	Writer(std::string& outStr, std::string_view indent = {});

	[[nodiscard]] ObjectWriter beginObject();
	[[nodiscard]] ArrayWriter  beginArray();

private:
	std::string*           m_outStr      = nullptr;
	int                    m_indentLevel = 0;
	const std::string_view m_indent;
	const std::string_view m_keySep;
	const std::string_view m_valueSep;
	const std::string_view m_newline;

	void writeIndent();
	void writePreValue(bool first);
	void writeObjectStart();
	void writeObjectEnd(bool hasItems);
	void writeArrayStart();
	void writeArrayEnd(bool hasItems);
	void writeObjectKey(std::string_view key);

	template<std::integral T>
	void write(T t)
	{
		if constexpr(std::is_signed_v<T>)
			write(static_cast<long long>(t));
		else
			write(static_cast<unsigned long long>(t));
	}

	void write(std::nullptr_t);
	void write(bool value);
	void write(long long value);
	void write(unsigned long long value);
	void write(double value);
	void write(const char* value);
	void write(const std::string& value);
	void write(std::string_view value);
	void write(const Value& value);
	void write(const Object& value);
	void write(const Array& value);
};

/*
 * JsonPrimitive
 */

template<typename T>
concept JsonPrimitive =
	std::is_null_pointer_v<T> ||
	std::is_same_v<T, bool> ||
	std::is_integral_v<T> ||
	std::is_floating_point_v<T> ||
	std::is_convertible_v<T, std::string_view> ||
	std::is_same_v<T, Value> ||
	std::is_same_v<T, Object> ||
	std::is_same_v<T, Array>;

/*
 * ObjectWriter
 */

class ObjectWriter{
	friend class Writer;
public:
	~ObjectWriter();

	ObjectWriter(const ObjectWriter&)            = delete;
	ObjectWriter& operator=(const ObjectWriter&) = delete;
	ObjectWriter(ObjectWriter&& other);
	ObjectWriter& operator=(ObjectWriter&& other);

	[[nodiscard]] ObjectWriter beginObject(std::string_view key);
	[[nodiscard]] ArrayWriter  beginArray(std::string_view key);

	template<JsonPrimitive T>
	void write(std::string_view key, const T& value)
	{
		m_writer->writePreValue(m_first);
		m_writer->writeObjectKey(key);
		m_writer->write(value);
		m_first = false;
	}

	template<typename... Args>
	void write(std::string_view key, const std::variant<Args...>& value)
	{
		std::visit([this, key](const auto& v){ this->write(key, v); }, value);
	}

private:
	Writer* m_writer = nullptr;
	bool    m_first  = true;

	ObjectWriter(Writer& writer);
};

/*
 * ArrayWriter
 */

class ArrayWriter{
	friend class Writer;
public:
	~ArrayWriter();

	ArrayWriter(const ArrayWriter&)            = delete;
	ArrayWriter& operator=(const ArrayWriter&) = delete;
	ArrayWriter(ArrayWriter&& other);
	ArrayWriter& operator=(ArrayWriter&& other);

	[[nodiscard]] ObjectWriter beginObject();
	[[nodiscard]] ArrayWriter  beginArray();

	template<JsonPrimitive T>
	void write(const T& value)
	{
		m_writer->writePreValue(m_first);
		m_writer->write(value);
		m_first = false;
	}

private:
	Writer* m_writer = nullptr;
	bool    m_first  = true;

	ArrayWriter(Writer& writer);
};

} // namespace lsp::json
