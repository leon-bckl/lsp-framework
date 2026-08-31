#include <cassert>
#include <charconv>
#include <iterator>
#include <limits>
#include "parser.h"

namespace lsp::json{
namespace{

auto isWhitespace(char c) -> bool
{
	return c <= 0x20; // Not 'correct' but good enough for this case
}

auto isDigit(char c) -> bool
{
	return c >= '0' && c <= '9';
}

auto isAlpha(char c) -> bool
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

auto isAlphanumeric(char c) -> bool
{
	return isAlpha(c) || isDigit(c);
}

void appendCodePointAsUtf8(std::string& str, unsigned int codepoint)
{
	if(codepoint < 0x80)
	{
		str += static_cast<char>(codepoint);
	}
	else if(codepoint < 0x800)
	{
		str += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if(codepoint < 0x10000)
	{
		str += static_cast<char>(0xE0 | ((codepoint >> 12) & 0xF));
		str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if(codepoint < 0x200000)
	{
		str += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x7));
		str += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		str += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		str += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else
	{
		str += "?";
	}
}

} // namespace

/*
 * Parser
 */

enum class Parser::State
{
	Value,
	Object,
	ObjectKey,
	Array
};

struct Parser::StateStackEntry{
	State  context;
	Value* value;
};

Parser::Parser(std::string_view text)
	: m_start{text.data()}
	, m_end{text.data() + text.size()}
	, m_pos{m_start}
{
	m_stateStack.reserve(10);
}

Parser::~Parser() = default;

auto Parser::atEnd() const -> bool
{
	return m_pos >= m_end;
}

auto Parser::textOffset(const char* pos) const -> std::size_t
{
	return static_cast<std::size_t>(std::distance(m_start, pos));
}

auto Parser::currentTextOffset() const -> std::size_t
{
	return textOffset(m_pos);
}

auto Parser::parse() -> Value
{
	Value result;

	pushState(State::Value, result);

	while(!m_stateStack.empty())
	{
		skipWhitespace();

		if(atEnd())
			throw ParseError("Unexpected end of input", currentTextOffset());

		switch(currentState())
		{
		case State::Value:
			handleValue();
			break;
		case State::Object:
			handleObject();
			break;
		case State::ObjectKey:
			handleObjectKey();
			break;
		case State::Array:
			handleArray();
			break;
		}
	}

	skipWhitespace();

	if(!atEnd())
		throw ParseError("Trailing characters", currentTextOffset());

	return result;
}

void Parser::reset()
{
	m_stateStack.clear();
	m_pos = m_start;
}

void Parser::handleValue()
{
	assert(currentState() == State::Value);

	if(*m_pos == '{')
	{
		++m_pos;
		currentValue() = Object{};
		pushState(State::Object, currentValue());
	}
	else if(*m_pos == '[')
	{
		++m_pos;
		currentValue() = Array{};
		pushState(State::Array, currentValue());
	}
	else
	{
		currentValue() = parseSimpleValue();
		popState();
	}
}

void Parser::handleObject()
{
	assert(currentState() == State::Object);

	if(*m_pos == '}')
	{
		++m_pos;
		popState(); // Object
		popState(); // Value
	}
	else
	{
		if(!currentValue().object().isEmpty())
		{
			if(*m_pos != ',')
				throw ParseError("Expected ','", currentTextOffset());

			const char* pos = m_pos;
			++m_pos;
			skipWhitespace();

			if(!atEnd() && *m_pos == '}')
				throw ParseError("Trailing ','", textOffset(pos));
		}

		pushState(State::ObjectKey, currentValue());
	}
}

void Parser::handleObjectKey()
{
	assert(currentState() == State::ObjectKey);

	const char* keyPos = m_pos;
	auto&       object = currentValue().object();
	const auto  key    = parseString();

	if(object.contains(key))
		throw ParseError("Duplicate key '" + key + "'", textOffset(keyPos));

	skipWhitespace();

	if(!atEnd() && *m_pos != ':')
		throw ParseError("Expected ':'", currentTextOffset());

	++m_pos;

	popState();
	pushState(State::Value, object.append(key, {}));
}

void Parser::handleArray()
{
	assert(currentState() == State::Array);

	if(*m_pos == ']')
	{
		++m_pos;
		popState(); // Array
		popState(); // Value
	}
	else
	{
		auto& array = currentValue().array();

		if(!array.empty())
		{
			if(*m_pos != ',')
				throw ParseError("Expected ','", currentTextOffset());

			const char* pos = m_pos;
			++m_pos;
			skipWhitespace();

			if(!atEnd() && *m_pos == ']')
				throw ParseError("Trailing ','", textOffset(pos));
		}

		pushState(State::Value, array.emplace_back());
	}
}

auto Parser::currentState() const -> Parser::State
{
	assert(!m_stateStack.empty());
	return m_stateStack.back().context;
}

auto Parser::currentValue() -> Value&
{
	assert(!m_stateStack.empty());
	return *m_stateStack.back().value;
}

void Parser::pushState(State state, Value& value)
{
	m_stateStack.push_back({state, &value});
}

void Parser::popState()
{
	assert(!m_stateStack.empty());
	m_stateStack.pop_back();
}

void Parser::skipWhitespace()
{
	while(!atEnd() && isWhitespace(*m_pos))
		++m_pos;
}

auto Parser::parseString() -> String
{
	if(atEnd() || *m_pos != '\"')
		throw ParseError("Expected string", currentTextOffset());

	const char* const stringStart = m_pos++;
	auto              hasEscape   = false;

	for(;;)
	{
		if(atEnd() || *m_pos == '\n')
			throw ParseError("Unmatched '\"'", textOffset(stringStart));

		if(!hasEscape && *m_pos == '"')
		{
			++m_pos;
			break;
		}

		hasEscape = !hasEscape && *m_pos == '\\';
		++m_pos;
	}

	auto str = std::string_view(stringStart, m_pos);

	if(str.size() > 0 && str.front() == '\"')
		str.remove_prefix(1);

	if(str.size() > 0 && str.back() == '\"')
		str.remove_suffix(1);

	std::string result;
	result.reserve(str.size());

	for(std::size_t i = 0; i < str.size(); ++i)
	{
		if(str[i] == '\\' && i != str.size() - 1)
		{
			++i;
			switch(str[i])
			{
			case 'b':
				result += '\b';
				break;
			case 't':
				result += '\t';
				break;
			case 'n':
				result += '\n';
				break;
			case 'f':
				result += '\f';
				break;
			case 'r':
				result += '\r';
				break;
			case '"':
				result += '"';
				break;
			case '/':
				result += '/';
				break;
			case '\\':
				result += '\\';
				break;
			case 'u':
				{
					const auto* first = str.data() + i + 1;
					const auto* last  = first + 4;

					if(last <= str.data() + str.size())
					{
						unsigned int codepoint;
						const auto [ptr, ec] = std::from_chars(first, last, codepoint, 16);

						if(ec == std::errc{} && ptr == last)
						{
							appendCodePointAsUtf8(result, codepoint);
							i += 4;
							break;
						}
					}
				}
				[[fallthrough]];
			default:
				{
					const char* invalidEscapeStart = stringStart + i;
					const char* invalidEscapeEnd   = invalidEscapeStart + 2;

					if(str[i] == 'u')
					{
						invalidEscapeEnd += 4;

						if(invalidEscapeEnd > str.data() + str.size())
							invalidEscapeEnd = str.data() + str.size();
					}

					throw ParseError(
						"Invalid escape sequence '" + std::string(invalidEscapeStart, invalidEscapeEnd) + '\'',
						textOffset(invalidEscapeStart));
				}
			}
		}
		else
		{
			result += str[i];
		}
	}

	return result;
}

auto Parser::parseNumber() -> Value
{
	const char* numberStart = m_pos;
	bool isDecimal = false;

	while(!atEnd() && (
				isAlphanumeric(*m_pos) ||
				*m_pos == '+' ||
				*m_pos == '-' ||
				*m_pos == '.' ||
				*m_pos == 'e' ||
				*m_pos == 'E'))
	{
		if(!isDecimal && (*m_pos == '.' || *m_pos == 'e' || *m_pos == 'E'))
			isDecimal = true;

		++m_pos;
	}

	if(isDecimal)
	{
		std::size_t   idx     = 0;
		const Decimal decimal = std::stod(std::string{numberStart, m_pos}, &idx);

		if(idx < static_cast<std::size_t>(std::distance(numberStart, m_pos)))
			throw ParseError("Invalid number value '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart));

		return decimal;
	}

	std::int64_t intValue;
	const auto [ptr, ec] = std::from_chars(numberStart, m_pos, intValue);

	if(ec != std::errc{} || ptr != m_pos)
		throw ParseError("Invalid number value '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart));

	if(intValue < std::numeric_limits<json::Integer>::min() || intValue > std::numeric_limits<json::Integer>::max())
		return static_cast<json::Decimal>(intValue);

	return static_cast<json::Integer>(intValue);
}

auto Parser::parseIdentifier() -> Value
{
	const char* idStart = m_pos;

	while(!atEnd() && isAlphanumeric(*m_pos))
		++m_pos;

	auto identifier = std::string_view(idStart, m_pos);

	if(identifier == "true")
		return Boolean(true);

	if(identifier == "false")
		return Boolean(false);

	if(identifier == "null")
		return Null();

	throw ParseError("Unexpected '" + std::string(identifier) + "'", textOffset(idStart));
}

auto Parser::parseSimpleValue() -> Value
{
	if(*m_pos == '\"')
		return parseString();

	if(isDigit(*m_pos) || *m_pos == '-')
		return parseNumber();

	if(isAlpha(*m_pos))
		return parseIdentifier();

	throw ParseError(std::string("Unexpected '") + * m_pos + "'", currentTextOffset());
}

} // namespace lsp::json
