#include <charconv>
#include <cmath>
#include "json.h"

namespace lsp::json{
namespace{

void stringifyImplementation(const Value& json, std::string& str, int indentLevel, std::string_view indent)
{
	const auto applyIndent = [&indentLevel, indent](std::string& out)
	{
		if(!indent.empty())
		{
			for(int i = 0; i < indentLevel; ++i)
				out += indent;
		}
	};

	std::string_view keySep{":"};
	std::string_view valueSep{","};
	std::string_view listStart;
	std::string_view listEnd;

	if(!indent.empty())
	{
		keySep = ": ";
		valueSep = ",\n";
		listStart = "\n";
		listEnd = "\n";
	}

	if(json.isNull())
	{
		str += "null";
	}
	else if(json.isBoolean())
	{
		str += json.boolean() ? "true" : "false";
	}
	else if(json.isInteger())
	{
		char buffer[32];
		const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), json.integer());
		str += std::string_view(buffer, ptr);
	}
	else if(json.isDecimal())
	{
		const auto value        = json.decimal();
		const auto absValue     = std::abs(value);
		const auto numberFormat = (absValue != 0.0 && (absValue < 1e-6 || absValue >= 1e21))
			? std::chars_format::scientific
			: std::chars_format::fixed;

		char buffer[32];
		const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, numberFormat);
		const auto numberStr = std::string_view(buffer, ptr);
		str += numberStr;

		if(numberStr.find_first_of(".eE") == std::string::npos)
			str += ".0";
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
			applyIndent(str);
			str += toStringLiteral(it->key());
			str += keySep;
			stringifyImplementation(it->value(), str, indentLevel, indent);
			++it;

			while(it != obj.end())
			{
				str += valueSep;
				applyIndent(str);
				str += toStringLiteral(it->key());
				str += keySep;
				stringifyImplementation(it->value(), str, indentLevel, indent);
				++it;
			}

			str += listEnd;
			--indentLevel;
			applyIndent(str);
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
			applyIndent(str);
			stringifyImplementation(*it, str, indentLevel, indent);
			++it;

			while(it != array.end())
			{
				str += valueSep;
				applyIndent(str);
				stringifyImplementation(*it, str, indentLevel, indent);
				++it;
			}

			str += listEnd;
			--indentLevel;
			applyIndent(str);
		}

		str += ']';
	}
}

} // namespace

std::string stringify(const Value& json, std::string_view indent)
{
	std::string str;
	stringifyImplementation(json, str, 0, indent);
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
