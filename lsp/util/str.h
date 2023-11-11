#pragma once

#include <cctype>
#include <string>
#include <vector>
#include <iterator>
#include <string_view>

namespace lsp::util::str{

[[nodiscard]] std::string_view trimViewLeft(std::string_view str);
[[nodiscard]] std::string_view trimViewRight(std::string_view str);
[[nodiscard]] std::string_view trimView(std::string_view str);

inline std::string_view trimViewLeft(std::string&&) = delete;
inline std::string_view trimViewRight(std::string&&) = delete;
inline std::string_view trimView(std::string&&) = delete;

[[nodiscard]] std::vector<std::string_view> splitView(std::string_view str, std::string_view separator, bool skipEmpty = false);
[[nodiscard]] std::vector<std::string_view> splitView(std::string&&, char, bool) = delete;

template<typename F>
[[nodiscard]] std::string join(const std::vector<std::string>& strings, const std::string& separator, F&& transform){
	std::string result;

	if(auto it = strings.begin(); it != strings.end()){
		result = transform(*it);
		++it;

		while(it != strings.end()){
			result += separator + transform(*it);
			++it;
		}
	}

	return result;
}

inline std::string join(const std::vector<std::string>& strings, const std::string& separator){
	return join(strings, separator, [](const std::string& str)->const std::string&{ return str; });
}

template<typename F>
[[nodiscard]] std::string join(const std::vector<std::string_view>& strings, const std::string& separator, F&& transform){
	std::string result;

	if(auto it = strings.begin(); it != strings.end()){
		result = transform(*it);
		++it;

		while(it != strings.end()){
			result += separator + transform(*it);
			++it;
		}
	}

	return result;
}

inline std::string join(const std::vector<std::string_view>& strings, const std::string& separator){
	return join(strings, separator, [](std::string_view str)->std::string{ return std::string{str}; });
}

[[nodiscard]] std::string replace(std::string_view str, std::string_view pattern, std::string_view replacement);
[[nodiscard]] std::string capitalize(std::string_view str);
[[nodiscard]] std::string uncapitalize(std::string_view str);
[[nodiscard]] std::string quote(std::string_view str);
[[nodiscard]] std::string escape(std::string_view str);
[[nodiscard]] std::string unescape(std::string_view str);

}
