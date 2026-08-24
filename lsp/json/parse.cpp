#include <cassert>
#include <charconv>
#include <iterator>
#include <limits>
#include "json.h"

namespace lsp::json{
namespace{

class Parser{
public:
	Parser(std::string_view text)
		: m_start{text.data()}
		, m_end{text.data() + text.size()}
		, m_pos{m_start}
	{
		m_stateStack.reserve(10);
	}

	bool atEnd() const
	{
		return m_pos >= m_end;
	}

	std::size_t textOffset(const char* pos) const
	{
		return static_cast<std::size_t>(std::distance(m_start, pos));
	}

	std::size_t currentTextOffset() const
	{
		return textOffset(m_pos);
	}

	Value parse()
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
			throw ParseError("Trailing characters in json", currentTextOffset());

		return result;
	}

	void reset()
	{
		m_stateStack.clear();
		m_pos = m_start;
	}

private:
	enum class State{
		Value,
		Object,
		ObjectKey,
		Array
	};

	struct StateStackEntry{
		State  context;
		Value* value;
	};

	std::vector<StateStackEntry> m_stateStack;
	const char* const            m_start = nullptr;
	const char* const            m_end   = nullptr;
	const char*                  m_pos   = nullptr;

	static bool isWhitespace(char c){ return c <= 0x20; } // Not 'correct' but good enough for this case
	static bool isDigit(char c){ return c >= '0' && c <= '9'; }
	static bool isAlpha(char c){ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
	static bool isAlphanumeric(char c){ return isAlpha(c) || isDigit(c); }

	void handleValue()
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

	void handleObject()
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

	void handleObjectKey()
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

	void handleArray()
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

	State currentState() const
	{
		assert(!m_stateStack.empty());
		return m_stateStack.back().context;
	}

	Value& currentValue()
	{
		assert(!m_stateStack.empty());
		return *m_stateStack.back().value;
	}

	void pushState(State state, Value& value)
	{
		m_stateStack.push_back({state, &value});
	}

	void popState()
	{
		assert(!m_stateStack.empty());
		m_stateStack.pop_back();
	}

	void skipWhitespace()
	{
		while(!atEnd() && isWhitespace(*m_pos))
			++m_pos;
	}

	String parseString()
	{
		if(atEnd() || *m_pos != '\"')
			throw ParseError("String expected", currentTextOffset());

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

		return fromStringLiteral(std::string_view(stringStart, m_pos));
	}

	Value parseNumber()
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
				throw ParseError("Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart));

			return decimal;
		}

		std::int64_t intValue;
		const auto [ptr, ec] = std::from_chars(numberStart, m_pos, intValue);

		if(ec != std::errc{} || ptr != m_pos)
			throw ParseError("Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart));

		if(intValue < std::numeric_limits<json::Integer>::min() || intValue > std::numeric_limits<json::Integer>::max())
			return static_cast<json::Decimal>(intValue);

		return static_cast<json::Integer>(intValue);
	}

	Value parseIdentifier()
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

	Value parseSimpleValue()
	{
		if(*m_pos == '\"')
			return parseString();

		if(isDigit(*m_pos) || *m_pos == '-')
			return parseNumber();

		if(isAlpha(*m_pos))
			return parseIdentifier();

		throw ParseError("Unexpected token", currentTextOffset());
	}
};

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

Value parse(std::string_view text)
{
	auto parser = Parser(text);

	return parser.parse();
}

std::string fromStringLiteral(std::string_view str)
{
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
			case '0':
				result += '\0';
				break;
			case 'a':
				result += '\a';
				break;
			case 'b':
				result += '\b';
				break;
			case 't':
				result += '\t';
				break;
			case 'n':
				result += '\n';
				break;
			case 'v':
				result += '\v';
				break;
			case 'f':
				result += '\f';
				break;
			case 'r':
				result += '\r';
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
						}
						else
						{
							const auto len = static_cast<std::size_t>(std::distance(first, ptr));
							result += "\\u";
							result += std::string_view(first, len);
							i += len;
						}
					}
					else
					{
						const auto len = static_cast<std::size_t>(std::distance(first, str.data() + str.size()));
						result += "\\u";
						result += std::string_view(first, len);
						i += len;
					}
					break;
				}
			default:
				result += str[i];
			}
		}
		else
		{
			result += str[i];
		}
	}

	return result;
}

} // namespace lsp::json
