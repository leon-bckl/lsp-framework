#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lspgen{

inline char toUpper(char c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

inline char toLower(char c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

inline std::string capitalizeString(std::string_view str)
{
	auto result = std::string(str);

	if(!result.empty())
		result[0] = static_cast<char>(toUpper(result[0]));

	return result;
}

inline std::string uncapitalizeString(std::string_view str)
{
	auto result = std::string(str);

	if(!result.empty())
		result[0] = static_cast<char>(toLower(result[0]));

	return result;
}

inline std::string replaceString(std::string_view str, std::string_view pattern, std::string_view replacement)
{
	auto result = std::string();
	result.reserve(str.size() + replacement.size());
	std::size_t srcIdx = 0;

	for(std::size_t idx = str.find(pattern); idx != std::string_view::npos; idx = str.find(pattern, srcIdx))
	{
		result += str.substr(srcIdx, idx - srcIdx);
		result += replacement;
		srcIdx  = idx + pattern.size();
	}

	result += str.substr(srcIdx);

	return result;
}

inline std::vector<std::string_view> splitStringView(std::string_view str, std::string_view separator, bool skipEmpty = false)
{
	auto        result = std::vector<std::string_view>();
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

inline std::string joinStrings(const std::vector<std::string_view>& strings, const std::string& separator, auto transform = [](std::string_view s){ return s; })
{
	auto result = std::string();

	if(auto it = strings.begin(); it != strings.end())
	{
		result = transform(*it);
		++it;

		while(it != strings.end())
		{
			result += separator + transform(*it);
			++it;
		}
	}

	return result;
}

} // namespace lspgen
