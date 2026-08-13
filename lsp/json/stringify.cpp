#include <algorithm>
#include "json.h"

namespace lsp::json{
namespace{

constexpr auto NullValueString  = std::string_view("null");
constexpr auto TrueValueString  = std::string_view("true");
constexpr auto FalseValueString = std::string_view("false");

void stringifyImplementation(const Value& json, std::string& str, std::size_t indentLevel, bool format)
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
		const auto& objMap = json.object().keyValueMap();

		str += '{';

		if(auto it = objMap.begin(); it != objMap.end())
		{
			str += listStart;
			++indentLevel;
			str += getIndent();
			str += toStringLiteral(it->first);
			str += keySep;
			stringifyImplementation(it->second, str, indentLevel, format);
			++it;

			while(it != objMap.end())
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

} // namespace

std::string stringify(const Value& json, bool format)
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
		case '\b':
			result += "\\b";
			break;
		case '\t':
			result += "\\t";
			break;
		case '\n':
			result += "\\n";
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
			if(static_cast<unsigned char>(c) < 0x20)
			{
				constexpr auto hexLookup = "0123456789ABCDEF";
				result += "\\u00";
				result += hexLookup[c >> 4];
				result += hexLookup[c & 0xF];
			}
			else
			{
				result += c;
			}
		}
	}

	result += '\"';

	return result;
}

} // namespace lsp::json
