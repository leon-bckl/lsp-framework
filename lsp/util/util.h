#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace lsp::util{
namespace str{

	[[nodiscard]] std::string_view trimViewLeft(std::string_view str){
		str.remove_prefix(std::distance(str.begin(), std::find_if(str.begin(), str.end(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	[[nodiscard]] std::string_view trimViewRight(std::string_view str){
		str.remove_suffix(std::distance(str.rbegin(), std::find_if(str.rbegin(), str.rend(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	[[nodiscard]] std::string_view trimView(std::string_view str){
		str = trimViewLeft(str);
		return trimViewRight(str);
	}

	std::string_view trimViewLeft(std::string&&) = delete;
	std::string_view trimViewRight(std::string&&) = delete;
	std::string_view trimView(std::string&&) = delete;

}
}
