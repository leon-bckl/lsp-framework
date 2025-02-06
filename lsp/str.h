#pragma once

#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "fileuri.h"

#ifndef _MSC_VER
#include <strings.h>
#endif

namespace lsp::str{

struct TransparentHash{
	using is_transparent = void;
	std::size_t operator()(const char* str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(std::string_view str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(const std::string& str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(const FileURI& uri) const{ return std::hash<std::string_view>{}(uri.path()); }
};

struct CaseInsensitiveHash{
	using is_transparent = void;

	std::size_t operator()(std::string_view str) const
	{
		std::string upper;
		upper.reserve(str.size());
		std::transform(str.cbegin(), str.cend(), std::back_inserter(upper), [](char c){ return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
		return std::hash<std::string>{}(upper);
	}

	std::size_t operator()(const char* str) const
	{
		return (*this)(std::string_view{str});
	}

	std::size_t operator()(const std::string& str) const
	{
		return (*this)(std::string_view{str});
	}

	std::size_t operator()(const FileURI& uri) const
	{
		return (*this)(std::string_view{uri.path()});
	}
};

struct CaseInsensitiveEqual{
	using is_transparent = void;

	bool operator()(std::string_view s1, std::string_view s2) const
	{
		return s1.size() == s2.size() &&
#ifdef _MSC_VER
		_strnicmp(s1.data(), s2.data(), s2.size()) == 0;
#else
		strncasecmp(s1.data(), s2.data(), s2.size()) == 0;
#endif
	}

	std::size_t operator()(const char* s1, const char* s2) const
	{
		return (*this)(std::string_view{s1}, std::string_view{s2});
	}

	std::size_t operator()(const std::string& s1, const std::string& s2) const
	{
		return (*this)(std::string_view{s1}, std::string_view{s2});
	}

	std::size_t operator()(const FileURI& u1, const FileURI& u2) const
	{
		return (*this)(std::string_view{u1.path()}, std::string_view{u2.path()});
	}
};

template<typename StringType>
concept CanBeStringMapKey = std::same_as<StringType, std::string> || std::same_as<StringType, std::string_view> || std::same_as<StringType, FileURI>;

/*
 * Unordered map and set types with that allow for faster lookup via string_view
 */
template<typename StringKeyType, typename ValueType, typename HashType = TransparentHash, typename EqualType = std::equal_to<>>
requires CanBeStringMapKey<StringKeyType>
using UnorderedMap = std::unordered_map<StringKeyType, ValueType, HashType, EqualType>;

template<typename StringKeyType, typename ValueType, typename HashType = TransparentHash, typename EqualType = std::equal_to<>>
requires CanBeStringMapKey<StringKeyType>
using UnorderedMultiMap = std::unordered_multimap<StringKeyType, ValueType, HashType, EqualType>;

template<typename StringKeyType, typename ValueType>
requires CanBeStringMapKey<StringKeyType>
using CaseInsensitiveUnorderedMap = UnorderedMap<StringKeyType, ValueType, CaseInsensitiveHash, CaseInsensitiveEqual>;

template<typename StringKeyType, typename ValueType>
requires CanBeStringMapKey<StringKeyType>
using CaseInsensitiveUnorderedMultiMap = UnorderedMultiMap<StringKeyType, ValueType, CaseInsensitiveHash, CaseInsensitiveEqual>;

template<typename StringType = std::string, typename HashType = TransparentHash, typename EqualType = std::equal_to<>>
requires CanBeStringMapKey<StringType>
using UnorderedSet = std::unordered_set<StringType, HashType, EqualType>;

template<typename StringType = std::string, typename HashType = TransparentHash, typename EqualType = std::equal_to<>>
requires CanBeStringMapKey<StringType>
using UnorderedMultiSet = std::unordered_multiset<StringType, HashType, EqualType>;

template<typename StringType = std::string>
requires CanBeStringMapKey<StringType>
using CaseInsensitiveUnorderedSet = UnorderedSet<StringType, CaseInsensitiveHash, CaseInsensitiveEqual>;

template<typename StringType = std::string>
requires CanBeStringMapKey<StringType>
using CaseInsensitiveUnorderedMultiSet = UnorderedMultiSet<StringType, CaseInsensitiveHash, CaseInsensitiveEqual>;

[[nodiscard]] std::string_view trimViewLeft(std::string_view str);
[[nodiscard]] std::string_view trimViewRight(std::string_view str);
[[nodiscard]] std::string_view trimView(std::string_view str);

inline std::string_view trimViewLeft(std::string&&) = delete;
inline std::string_view trimViewRight(std::string&&) = delete;
inline std::string_view trimView(std::string&&) = delete;

[[nodiscard]] std::string trimLeft(std::string_view str);
[[nodiscard]] std::string trimLeft(std::string&& str);
[[nodiscard]] std::string trimRight(std::string_view str);
[[nodiscard]] std::string trimRight(std::string&& str);
[[nodiscard]] std::string trim(std::string_view str);

[[nodiscard]] std::vector<std::string_view> splitView(std::string_view str, std::string_view separator, bool skipEmpty = false);
[[nodiscard]] std::vector<std::string_view> splitView(std::string&&, char, bool) = delete;

template<typename F>
[[nodiscard]] std::string join(const std::vector<std::string>& strings, const std::string& separator, F&& transform)
{
	std::string result;

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

[[nodiscard]] inline std::string join(const std::vector<std::string>& strings, const std::string& separator)
{
	return join(strings, separator, [](const std::string& str)->const std::string&{ return str; });
}

template<typename F>
[[nodiscard]] std::string join(const std::vector<std::string_view>& strings, const std::string& separator, F&& transform)
{
	std::string result;

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

inline std::string join(const std::vector<std::string_view>& strings, const std::string& separator)
{
	return join(strings, separator, [](std::string_view str)->std::string{ return std::string{str}; });
}

[[nodiscard]] std::string replace(std::string_view str, std::string_view pattern, std::string_view replacement);
[[nodiscard]] std::string lower(std::string_view str);
[[nodiscard]] std::string upper(std::string_view str);
[[nodiscard]] std::string capitalize(std::string_view str);
[[nodiscard]] std::string uncapitalize(std::string_view str);
[[nodiscard]] std::string quote(std::string_view str);
[[nodiscard]] std::string escape(std::string_view str);
[[nodiscard]] std::string unescape(std::string_view str);

} // namespace lsp::str
