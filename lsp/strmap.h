#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <lsp/fileuri.h>

namespace lsp{

struct TransparentStrHash{
	using is_transparent = void;
	std::size_t operator()(const char* str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(std::string_view str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(const std::string& str) const{ return std::hash<std::string_view>{}(str); }
	std::size_t operator()(const FileURI& uri) const{ return std::hash<std::string_view>{}(uri.path()); }
};

template<typename K, typename V, typename Hash = TransparentStrHash, typename Equal = std::equal_to<>>
requires std::same_as<K, const char*> ||
         std::same_as<K, std::string_view> ||
         std::same_as<K, std::string> ||
         std::same_as<K, FileURI>
using StrMap = std::unordered_map<K, V, Hash, Equal>;

} // namespace lsp
