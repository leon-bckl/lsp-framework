#pragma once

#include <fstream>
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
		result[0] = toUpper(result[0]);

	return result;
}

inline auto uncapitalizeString(std::string_view str) -> std::string
{
	auto result = std::string(str);

	if(!result.empty())
		result[0] = toLower(result[0]);

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

inline auto readFileContent(const std::string& fileName, bool mayFail = false) -> std::string
{
	auto file = std::ifstream(fileName, std::ios::binary);

	if(!file)
	{
		if(mayFail)
			return {};

		throw std::runtime_error("Failed to read file '" + fileName + '\'');
	}

	file.seekg(0, std::ios::end);
	const auto fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string text;
	text.resize(static_cast<std::string::size_type>(fileSize));
	file.read(&text[0], fileSize);

	return text;
}

inline void writeFileContent(const std::string& fileName, std::string_view content)
{
	auto file = std::ofstream(fileName, std::ios::trunc | std::ios::binary);

	if(!file)
		throw std::runtime_error("Failed to write file '" + fileName + '\'');

	file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

} // namespace lspgen
