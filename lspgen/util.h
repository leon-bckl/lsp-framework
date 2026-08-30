#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lspgen{

inline auto toUpper(char c) -> char
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

inline auto toLower(char c) -> char
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

inline auto stringToLower(std::string_view str) -> std::string
{
	auto lower = std::string();
	lower.resize(str.size());

	for(std::size_t i = 0 ; i < str.size(); ++i)
		lower[i] = toLower(str[i]);

	return lower;
}

inline auto stringToUpper(std::string_view str) -> std::string
{
	auto upper = std::string();
	upper.resize(str.size());

	for(std::size_t i = 0 ; i < str.size(); ++i)
		upper[i] = toUpper(str[i]);

	return upper;
}

inline auto capitalizeString(std::string_view str) -> std::string
{
	auto result = std::string(str);

	if(!result.empty())
		result[0] = static_cast<char>(toUpper(result[0]));

	return result;
}

inline auto uncapitalizeString(std::string_view str) -> std::string
{
	auto result = std::string(str);

	if(!result.empty())
		result[0] = static_cast<char>(toLower(result[0]));

	return result;
}

inline auto replaceString(std::string_view str, std::string_view pattern, std::string_view replacement) -> std::string
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

inline auto splitStringView(std::string_view str, std::string_view separator, bool skipEmpty = false) -> std::vector<std::string_view>
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

template<typename T, typename F = std::string_view(*)(std::string_view)>
requires std::convertible_to<T, std::string_view>
auto joinStrings(const std::vector<T>& strings, const std::string& separator, F&& transform = [](std::string_view s){ return s; }) -> std::string
{
	auto result = std::string();

	if(auto it = strings.begin(); it != strings.end())
	{
		result = transform(*it);
		++it;

		while(it != strings.end())
		{
			result += separator;
			result += transform(*it);
			++it;
		}
	}

	return result;
}

} // namespace lspgen
