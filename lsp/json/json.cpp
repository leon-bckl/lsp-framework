#include "json.h"

#include <limits>
#include <vector>
#include <cassert>
#include <string>
#include <charconv>
#include <algorithm>
#include <lsp/str.h>

namespace lsp::json{
namespace{

constexpr std::string_view Null{"null"};
constexpr std::string_view True{"true"};
constexpr std::string_view False{"false"};

/*
 * Parser
 */

std::size_t textOffset(const char* start, const char* pos)
{
	return static_cast<std::size_t>(std::distance(start, pos));
}

class Parser{
public:
	Parser(std::string_view text) :
		m_start{text.data()},
		m_end{text.data() + text.size()},
		m_pos{m_start}
	{
		m_stateStack.reserve(10);
	}

	Any parse()
	{
		Any result;

		pushState(State::Value, result);

		while(!m_stateStack.empty())
		{
			skipWhitespace();

			if(m_pos >= m_end)
				throw ParseError{"Unexpected end of input", textOffset(m_start, m_pos)};

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
					throw ParseError{"Expected ','", textOffset(m_start, m_pos)};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(m_pos < m_end && *m_pos == '}')
					throw ParseError{"Trailing ','", textOffset(m_start, pos)};
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
			throw ParseError{"Duplicate key '" + key + "'", textOffset(m_start, keyPos)};

		skipWhitespace();

		if(m_pos < m_end && *m_pos != ':')
			throw ParseError{"Expected ':'", textOffset(m_start, m_pos)};

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
					throw ParseError{"Expected ','", textOffset(m_start, m_pos)};

				const char* pos = m_pos;
				++m_pos;
				skipWhitespace();

				if(m_pos < m_end && *m_pos == ']')
					throw ParseError{"Trailing ','", textOffset(m_start, pos)};
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
		while(m_pos < m_end && std::isspace(*m_pos))
			++m_pos;
	}

	String parseString()
	{
		if(m_pos >= m_end || *m_pos != '\"')
			throw ParseError{"String expected", textOffset(m_start, m_pos)};

		const char* stringStart = ++m_pos;

		bool escaping = false;
		while(*m_pos != '\"' || escaping)
		{
			if (!escaping && *m_pos == '\\')
				escaping = true;
			else // Already escaping or no '\'
				escaping = false;

			++m_pos;

			if(m_pos >= m_end || *m_pos == '\n')
				throw ParseError{"Unmatched '\"'", textOffset(m_start, m_pos)};
		}

		const char* stringEnd = m_pos++;

		return str::unescape(std::string_view{stringStart, stringEnd});
	}

	Any parseNumber()
	{
		const char* numberStart = m_pos;
		bool isDecimal = false;

		while(m_pos < m_end && (std::isalnum(*m_pos) || *m_pos == '-' || *m_pos == '.' || *m_pos == 'e' || *m_pos == 'E'))
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
				throw ParseError{"Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(m_start, numberStart)};

			return decimal;
		}

		std::int64_t intValue;
		const auto [ptr, ec] = std::from_chars(numberStart, m_pos, intValue);

		if(ec != std::errc{} || ptr != m_pos)
			throw ParseError{"Invalid number value: '" + std::string{numberStart, m_pos} + "'", textOffset(m_start, numberStart)};

		if(intValue < std::numeric_limits<json::Integer>::min() || intValue > std::numeric_limits<json::Integer>::max())
			return static_cast<json::Decimal>(intValue);

		return static_cast<json::Integer>(intValue);
	}

	Any parseIdentifier()
	{
		const char* idStart = m_pos;

		while(m_pos < m_end && std::isalnum(*m_pos))
			++m_pos;

		std::string_view id{ idStart, static_cast<std::size_t>(std::distance(idStart, m_pos)) };

		if(id == True)
			return true;

		if(id == False)
			return false;

		if(id == Null)
			return {};

		throw ParseError{"Unexpected '" + std::string{idStart, m_pos} + "'", textOffset(m_start, m_pos)};
	}

	Any parseSimpleValue()
	{
		if(*m_pos == '\"')
			return parseString();

		if(std::isdigit(*m_pos) || *m_pos == '-')
			return parseNumber();

		if(std::isalpha(*m_pos))
			return parseIdentifier();

		throw ParseError{"Unexpected token", textOffset(m_start, m_pos)};
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
		str += Null;
	}
	else if(json.isBoolean())
	{
		str += json.boolean() ? True : False;
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
		str += str::quote(str::escape(json.string()));
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
			str += str::quote(str::escape(it->first));
			str += keySep;
			stringifyImplementation(it->second, str, indentLevel, format);
			++it;

			while(it != obj.end())
			{
				str += valueSep;
				str += getIndent();
				str += str::quote(str::escape(it->first));
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

} // namespace lsp::json
