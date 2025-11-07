#include <algorithm>
#include <cassert>
#include <charconv>
#include <iterator>
#include <limits>
#include <string>
#include <vector>
#include <lsp/json/json.h>

namespace lsp::json{
namespace{

constexpr std::string_view NullValueString{"null"};
constexpr std::string_view TrueValueString{"true"};
constexpr std::string_view FalseValueString{"false"};

/*
 * Parser
 */

class Parser{
public:
	Parser(std::string_view text) :
		m_start{text.data()},
		m_end{text.data() + text.size()},
		m_pos{m_start}
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

	Any parse()
	{
		Any result;

		pushState(State::Value, result);

		while(!m_stateStack.empty())
		{
			skipWhitespace();

			if(atEnd())
				throw ParseError{"Unexpected end of input", currentTextOffset()};

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
			throw ParseError{"Trailing characters in json", currentTextOffset()};

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
		State context;
		Any*    value;
	};

	std::vector<StateStackEntry> m_stateStack;
	const char* const            m_start = nullptr;
	const char* const            m_end   = nullptr;
	const char*                  m_pos   = nullptr;

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
			if(!currentValue().object().empty())
			{
				if(*m_pos != ',')
					throw ParseError{"Expected ','", currentTextOffset()};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(!atEnd() && *m_pos == '}')
					throw ParseError{"Trailing ','", textOffset(pos)};
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
			throw ParseError{"Duplicate key '" + key + "'", textOffset(keyPos)};

		skipWhitespace();

		if(!atEnd() && *m_pos != ':')
			throw ParseError{"Expected ':'", currentTextOffset()};

		++m_pos;

		popState();
		pushState(State::Value, object[key]);
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
					throw ParseError{"Expected ','", currentTextOffset()};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(!atEnd() && *m_pos == ']')
					throw ParseError{"Trailing ','", textOffset(pos)};
			}

			pushState(State::Value, array.emplace_back());
		}
	}

	State currentState() const
	{
		assert(!m_stateStack.empty());
		return m_stateStack.back().context;
	}

	Any& currentValue()
	{
		assert(!m_stateStack.empty());
		return *m_stateStack.back().value;
	}

	void pushState(State state, Any& value)
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
		while(!atEnd() && std::isspace(static_cast<unsigned char>(*m_pos)))
			++m_pos;
	}

	String parseString()
	{
		if(atEnd() || *m_pos != '\"')
			throw ParseError{"String expected", currentTextOffset()};

		const char* stringStart = m_pos++;

		bool escaping = false;
		while(*m_pos != '\"' || escaping)
		{
			if(!escaping && *m_pos == '\\')
				escaping = true;
			else // Already escaping or no '\'
				escaping = false;

			++m_pos;

			if(atEnd() || *m_pos == '\n')
				throw ParseError{"Unmatched '\"'", currentTextOffset()};
		}

		const char* stringEnd = ++m_pos;

		return fromStringLiteral(std::string_view{stringStart, stringEnd});
	}

	Any parseNumber()
	{
		const char* numberStart = m_pos;
		bool isDecimal = false;

		while(!atEnd() && (
		      std::isalnum(static_cast<unsigned char>(*m_pos)) ||
		      *m_pos == '-' ||
		      *m_pos == '.' ||
		      *m_pos == 'e' ||
		      *m_pos == 'E')
		)
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
				throw ParseError{"Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart)};

			return decimal;
		}

		std::int64_t intValue;
		const auto [ptr, ec] = std::from_chars(numberStart, m_pos, intValue);

		if(ec != std::errc{} || ptr != m_pos)
			throw ParseError{"Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(numberStart)};

		if(intValue < std::numeric_limits<json::Integer>::min() || intValue > std::numeric_limits<json::Integer>::max())
			return static_cast<json::Decimal>(intValue);

		return static_cast<json::Integer>(intValue);
	}

	Any parseIdentifier()
	{
		const char* idStart = m_pos;

		while(!atEnd() && std::isalnum(static_cast<unsigned char>(*m_pos)))
			++m_pos;

		auto identifier = std::string_view(idStart, m_pos);

		if(identifier == TrueValueString)
			return Boolean(true);

		if(identifier == FalseValueString)
			return Boolean(false);

		if(identifier == NullValueString)
			return Null();

		throw ParseError{"Unexpected '" + std::string(identifier) + "'", currentTextOffset()};
	}

	Any parseSimpleValue()
	{
		if(*m_pos == '\"')
			return parseString();

		if(std::isdigit(static_cast<unsigned char>(*m_pos)) || *m_pos == '-')
			return parseNumber();

		if(std::isalpha(static_cast<unsigned char>(*m_pos)))
			return parseIdentifier();

		throw ParseError{"Unexpected token", currentTextOffset()};
	}
};

void stringifyImplementation(const Any& json, std::string& str, std::size_t indentLevel, bool format)
{
	const auto getIndent = [&indentLevel, format]()
	{
		if(!format)
			return std::string_view{};

		static constexpr std::string_view Tabs{"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"};
		return Tabs.substr(0, std::min(indentLevel, Tabs.size()));
	};

	std::string_view keySep{":"};
	std::string_view valueSep{","};
	std::string_view listStart;
	std::string_view listEnd;

	if(format)
	{
		keySep = ": ";
		valueSep = ",\n";
		listStart = "\n";
		listEnd = "\n";
	}

	if(json.isNull())
	{
		str += NullValueString;
	}
	else if(json.isBoolean())
	{
		str += json.boolean() ? TrueValueString : FalseValueString;
	}
	else if(json.isInteger())
	{
		str += std::to_string(json.integer());
	}
	else if(json.isDecimal())
	{
		auto numberStr = std::to_string(json.decimal());

		for(std::size_t i = numberStr.size(); i > 2; --i)
		{
			if(numberStr[i] != '0' || numberStr[i - 1] == '.')
				break;

			numberStr.pop_back();
		}

		str += numberStr;
	}
	else if(json.isString())
	{
		str += toStringLiteral(json.string());
	}
	else if(json.isObject())
	{
		const auto& obj = json.object();

		str += '{';

		if(auto it = obj.begin(); it != obj.end())
		{
			str += listStart;
			++indentLevel;
			str += getIndent();
			str += toStringLiteral(it->first);
			str += keySep;
			stringifyImplementation(it->second, str, indentLevel, format);
			++it;

			while(it != obj.end())
			{
				str += valueSep;
				str += getIndent();
				str += toStringLiteral(it->first);
				str += keySep;
				stringifyImplementation(it->second, str, indentLevel, format);
				++it;
			}

			str += listEnd;
			--indentLevel;
			str += getIndent();
		}

		str += '}';
	}
	else if(json.isArray())
	{
		const auto& array = json.array();

		str += '[';

		if(auto it = array.begin(); it != array.end())
		{
			str += listStart;
			++indentLevel;
			str += getIndent();
			stringifyImplementation(*it, str, indentLevel, format);
			++it;

			while(it != array.end())
			{
				str += valueSep;
				str += getIndent();
				stringifyImplementation(*it, str, indentLevel, format);
				++it;
			}

			str += listEnd;
			--indentLevel;
			str += getIndent();
		}

		str += ']';
	}
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

Any& Object::get(std::string_view key)
{
	if(const auto it = find(key); it != end())
		return it->second;

	throw TypeError{"Missing key '" + std::string{key} + '\''};
}

const Any& Object::get(std::string_view key) const
{
	if(const auto it = find(key); it != end())
		return it->second;

	throw TypeError{"Missing key '" + std::string{key} + '\''};
}

Any parse(std::string_view text)
{
	Parser parser{text};

	return parser.parse();
}

std::string stringify(const Any& json, bool format)
{
	std::string str;
	stringifyImplementation(json, str, 0, format);
	return str;
}

std::string toStringLiteral(std::string_view str)
{
	std::string result;
	result.reserve(str.size() + 2);
	result += '\"';

	for(const char c : str)
	{
		switch(c)
		{
		case '\0':
			result += "\\0";
			break;
		case '\a':
			result += "\\a";
			break;
		case '\b':
			result += "\\b";
			break;
		case '\t':
			result += "\\t";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\v':
			result += "\\v";
			break;
		case '\f':
			result += "\\f";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		default:
			result += c;
		}
	}

	result += '\"';

	return result;
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
