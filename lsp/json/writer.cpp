#include <charconv>
#include <cmath>
#include <utility>
#include "writer.h"
#include "json.h"

namespace lsp::json{
namespace{

void appendStringLiteral(std::string_view str, std::string& out)
{
	out.reserve(out.size() + str.size() + 2);
	out += '\"';

	for(const char c : str)
	{
		switch(c)
		{
		case '\b':
			out += "\\b";
			break;
		case '\t':
			out += "\\t";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		default:
			if(static_cast<unsigned char>(c) < 0x20)
			{
				constexpr auto hexLookup = "0123456789ABCDEF";
				out += "\\u00";
				out += hexLookup[c >> 4];
				out += hexLookup[c & 0xF];
			}
			else
			{
				out += c;
			}
		}
	}

	out += '\"';
}

} // namespace

/*
 * Writer
 */

Writer::Writer(std::string& outStr, std::string_view indent)
	: m_outStr(&outStr)
	, m_indent(indent)
	, m_keySep(indent.empty() ? ":" : ": ")
	, m_valueSep(indent.empty() ? "," : ",\n")
	, m_newline(indent.empty() ? "" : "\n")
{
}

void Writer::write(std::nullptr_t)
{
	*m_outStr += "null";
}

void Writer::write(bool value)
{
	*m_outStr += value ? "true" : "false";
}

void Writer::write(long long value)
{
	char buffer[32];
	const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
	*m_outStr += std::string_view(buffer, ptr);
}

void Writer::write(unsigned long long value)
{
	char buffer[32];
	const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
	*m_outStr += std::string_view(buffer, ptr);
}

void Writer::write(double value)
{
	if(!std::isfinite(value))
	{
		*m_outStr += "null"; // There's no nan/inf in json
		return;
	}

	const auto absValue     = std::abs(value);
	const auto numberFormat = (absValue != 0.0 && (absValue < 1e-6 || absValue >= 1e21))
	 ? std::chars_format::scientific
	 : std::chars_format::fixed;

	char buffer[32];
	const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, numberFormat);
	const auto numberStr = std::string_view(buffer, ptr);
	*m_outStr += numberStr;

	if(numberStr.find_first_of(".eE") == std::string::npos)
		*m_outStr += ".0";
}

void Writer::write(const char* value)
{
	appendStringLiteral(value, *m_outStr);
}

void Writer::write(const std::string& value)
{
	appendStringLiteral(value, *m_outStr);
}

void Writer::write(std::string_view value)
{
	appendStringLiteral(value, *m_outStr);
}

void Writer::write(const Value& value)
{
	std::visit([this](const auto& v){ write(v); }, value.variant());
}

void Writer::write(const Object& value)
{
	auto objectWriter = beginObject();

	for(const auto& [k, v] : value)
		objectWriter.write(k, v);
}

void Writer::write(const Array& value)
{
	auto arrayWriter = beginArray();

	for(const auto& v : value)
		arrayWriter.write(v);
}

auto Writer::beginObject() -> ObjectWriter
{
	return ObjectWriter(*this);
}

auto Writer::beginArray() -> ArrayWriter
{
	return ArrayWriter(*this);
}

void Writer::writeIndent()
{
	if(!m_indent.empty())
	{
		for(int i = 0; i < m_nestingLevel; ++i)
			*m_outStr += m_indent;
	}
}

void Writer::writeObjectStart()
{
	*m_outStr += '{';
	++m_nestingLevel;
}

void Writer::writeObjectEnd(bool hasItems)
{
	--m_nestingLevel;

	if(hasItems)
	{
		*m_outStr += m_newline;
		writeIndent();
	}

	*m_outStr += '}';
}

void Writer::writeArrayStart()
{
	*m_outStr += '[';
	++m_nestingLevel;
}

void Writer::writeArrayEnd(bool hasItems)
{
	--m_nestingLevel;

	if(hasItems)
	{
		*m_outStr += m_newline;
		writeIndent();
	}

	*m_outStr += ']';
}

void Writer::writeObjectKey(std::string_view key)
{
	appendStringLiteral(key, *m_outStr);
	*m_outStr += m_keySep;
}

void Writer::writePreValue(bool first)
{
	if(first)
		*m_outStr += m_newline;
	else
		*m_outStr += m_valueSep;

	writeIndent();
}

/*
 * ObjectWriter
 */

ObjectWriter::ObjectWriter(Writer& writer)
	: m_writer(&writer)
{
	m_writer->writeObjectStart();
}

ObjectWriter::~ObjectWriter()
{
	finalize();
}

ObjectWriter::ObjectWriter(ObjectWriter&& other) noexcept
	: m_writer(std::exchange(other.m_writer, nullptr))
	, m_first(other.m_first)
{
}

ObjectWriter& ObjectWriter::operator=(ObjectWriter&& other) noexcept
{
	finalize();

	m_writer = std::exchange(other.m_writer, nullptr);
	m_first  = other.m_first;

	return *this;
}

void ObjectWriter::finalize()
{
	if(m_writer)
	{
		m_writer->writeObjectEnd(!m_first);
		m_writer = nullptr;
	}
}

auto ObjectWriter::beginObject(std::string_view key) -> ObjectWriter
{
	m_writer->writePreValue(m_first);
	m_first = false;
	m_writer->writeObjectKey(key);
	return m_writer->beginObject();
}

auto ObjectWriter::beginArray(std::string_view key) -> ArrayWriter
{
	m_writer->writePreValue(m_first);
	m_first = false;
	m_writer->writeObjectKey(key);
	return m_writer->beginArray();
}

/*
 * ArrayWriter
 */

ArrayWriter::ArrayWriter(Writer& writer)
	: m_writer(&writer)
{
	m_writer->writeArrayStart();
}

ArrayWriter::~ArrayWriter()
{
	finalize();
}

ArrayWriter::ArrayWriter(ArrayWriter&& other) noexcept
	: m_writer(std::exchange(other.m_writer, nullptr))
	, m_first(other.m_first)
{
}

ArrayWriter& ArrayWriter::operator=(ArrayWriter&& other) noexcept
{
	finalize();

	m_writer = std::exchange(other.m_writer, nullptr);
	m_first  = other.m_first;

	return *this;
}

void ArrayWriter::finalize()
{
	if(m_writer)
	{
		m_writer->writeArrayEnd(!m_first);
		m_writer = nullptr;
	}
}

ObjectWriter ArrayWriter::beginObject()
{
	m_writer->writePreValue(m_first);
	m_first = false;
	return m_writer->beginObject();
}

ArrayWriter ArrayWriter::beginArray()
{
	m_writer->writePreValue(m_first);
	m_first = false;
	return m_writer->beginArray();
}

} // namespace lsp::json
