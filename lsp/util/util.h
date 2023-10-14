#pragma once

#include <string_view>
#include <cctype>

namespace lsp::util{
namespace str{

	std::string_view trimViewLeft(std::string_view str){
		str.remove_prefix(std::distance(str.begin(), std::find_if(str.begin(), str.end(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	std::string_view trimViewRight(std::string_view str){
		str.remove_suffix(std::distance(str.rbegin(), std::find_if(str.rbegin(), str.rend(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	std::string_view trimView(std::string_view str){
		str = trimViewLeft(str);
		return trimViewRight(str);
	}

}
}
