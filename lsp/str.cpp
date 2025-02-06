#include "str.h"

#include <cctype>
#include <iterator>
#include <algorithm>

namespace lsp::str{

std::string_view trimViewLeft(std::string_view str)
{
	str.remove_prefix(std::distance(str.begin(), std::find_if(str.begin(), str.end(), [](char c){ return !std::isspace(static_cast<unsigned char>(c)); })));
	return str;
}

std::string_view trimViewRight(std::string_view str)
{
	str.remove_suffix(std::distance(str.rbegin(), std::find_if(str.rbegin(), str.rend(), [](char c){ return !std::isspace(static_cast<unsigned char>(c)); })));
	return str;
}

std::string_view trimView(std::string_view str)
{
	str = trimViewLeft(str);
	return trimViewRight(str);
}

std::string trimLeft(std::string_view str)
{
	return trimLeft(std::string{str});
}

std::string trimLeft(std::string&& str)
{
	auto trimmed = std::move(str);
	trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](char c){ return !std::isspace(static_cast<unsigned char>(c)); }));
	return trimmed;
}

std::string trimRight(std::string_view str)
{
	return trimRight(std::string{str});
}

std::string trimRight(std::string&& str)
{
	auto trimmed = std::move(str);
	trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](char c){ return !std::isspace(static_cast<unsigned char>(c)); }).base(), trimmed.end());
	return trimmed;
}

std::string trim(std::string_view str)
{
	return trimRight(trimLeft(str));
}

std::vector<std::string_view> splitView(std::string_view str, std::string_view separator, bool skipEmpty)
{
	std::vector<std::string_view> result;
	std::size_t srcIdx = 0;

	for(std::size_t idx = str.find(separator); idx != std::string_view::npos; idx = str.find(separator, srcIdx))
	{
		const auto part = str.substr(srcIdx, idx - srcIdx);
		srcIdx = idx + separator.size();

		if(part.empty() && skipEmpty)
			continue;

		result.push_back(part);
	}

	if(srcIdx < str.size())
		result.push_back(str.substr(srcIdx));

	return result;
}

std::string replace(std::string_view str, std::string_view pattern, std::string_view replacement)
{
	std::string result;
	result.reserve(str.size() + replacement.size());
	std::size_t srcIdx = 0;

	for(std::size_t idx = str.find(pattern); idx != std::string_view::npos; idx = str.find(pattern, srcIdx))
	{
		result += str.substr(srcIdx, idx - srcIdx);
		result += replacement;
		srcIdx = idx + pattern.size();
	}

	result += str.substr(srcIdx);

	return result;
}

std::string lower(std::string_view str)
{
	std::string result;
	result.reserve(str.size());
	std::transform(str.begin(), str.end(), std::back_inserter(result), [](char c){ return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return result;
}

std::string upper(std::string_view str)
{
	std::string result;
	result.reserve(str.size());
	std::transform(str.begin(), str.end(), std::back_inserter(result), [](char c){ return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
	return result;
}

std::string capitalize(std::string_view str)
{
	std::string result{str};

	if(!result.empty())
		result[0] = static_cast<char>(std::toupper(result[0]));

	return result;
}

std::string uncapitalize(std::string_view str)
{
	std::string result{str};

	if(!result.empty())
		result[0] = static_cast<char>(std::tolower(result[0]));

	return result;
}

std::string quote(std::string_view str)
{
	std::string result;
	result.reserve(str.size() + 2);
	result += '\"';
	result += str;
	result += '\"';
	return result;
}

std::string escape(std::string_view str)
{
	std::string result;
	result.reserve(str.size());

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

	return result;
}

std::string unescape(std::string_view str)
{
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

} // namespace lsp::str
