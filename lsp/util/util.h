#pragma once

#include <tuple>
#include <cctype>
#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace lsp::util{
namespace str{

	[[nodiscard]] inline std::string_view trimViewLeft(std::string_view str){
		str.remove_prefix(std::distance(str.begin(), std::find_if(str.begin(), str.end(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	[[nodiscard]] inline std::string_view trimViewRight(std::string_view str){
		str.remove_suffix(std::distance(str.rbegin(), std::find_if(str.rbegin(), str.rend(), [](char c){ return !std::isspace(c); })));
		return str;
	}

	[[nodiscard]] inline std::string_view trimView(std::string_view str){
		str = trimViewLeft(str);
		return trimViewRight(str);
	}

	inline std::string_view trimViewLeft(std::string&&) = delete;
	inline std::string_view trimViewRight(std::string&&) = delete;
	inline std::string_view trimView(std::string&&) = delete;

	[[nodiscard]] inline std::vector<std::string_view> splitView(std::string_view str, std::string_view separator, bool skipEmpty = false){
		std::vector<std::string_view> result;
		std::size_t srcIdx = 0;

		for(std::size_t idx = str.find(separator); idx != std::string_view::npos; idx = str.find(separator, srcIdx)){
			auto part = str.substr(srcIdx, idx - srcIdx);
			srcIdx = idx + separator.size();

			if(part.empty() && skipEmpty)
				continue;

			result.push_back(part);
		}

		if(srcIdx < str.size())
			result.push_back(str.substr(srcIdx));

		return result;
	}

	inline std::vector<std::string_view> splitView(std::string&&, char, bool) = delete;

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

	[[nodiscard]] inline std::string replace(std::string_view str, std::string_view pattern, std::string_view replacement){
		std::string result;
		result.reserve(str.size() + replacement.size());
		std::size_t srcIdx = 0;

		for(std::size_t idx = str.find(pattern); idx != std::string_view::npos; idx = str.find(pattern, srcIdx)){
			result += str.substr(srcIdx, idx - srcIdx);
			result += replacement;
			srcIdx = idx + pattern.size();
		}

		result += str.substr(srcIdx);

		return result;
	}

	[[nodiscard]] inline std::string capitalize(std::string_view str){
		std::string result{str};

		if(!result.empty())
			result[0] = static_cast<char>(std::toupper(result[0]));

		return result;
	}

	[[nodiscard]] inline std::string uncapitalize(std::string_view str){
		std::string result{str};

		if(!result.empty())
			result[0] = static_cast<char>(std::tolower(result[0]));

		return result;
	}

	[[nodiscard]] inline std::string quote(std::string_view str){
		std::string result;
		result.reserve(str.size() + 2);
		result += '\"';
		result += str;
		result += '\"';
		return result;
	}

	[[nodiscard]] inline std::string escape(std::string_view str){
		std::string result;
		result.reserve(str.size());

		for(char c : str){
			switch(c){
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

	[[nodiscard]] inline std::string unescape(std::string_view str){
		std::string result;
		result.reserve(str.size());

		for(std::size_t i = 0; i < str.size(); ++i){
			if(str[i] == '\\' && i != str.size() - 1){
				++i;
				switch(str[i]){
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
			}else{
				result += str[i];
			}
		}

		return result;
	}

}

namespace tuple{
namespace impl{

template<typename T, typename F, int idx = std::tuple_size_v<typename std::decay<T>::type> - 1>
struct Visitor{
	void visit(T&& tuple, F&& f){
		Visitor<T, F, idx - 1> visitor;
		visitor.visit(std::forward<T>(tuple), std::forward<F>(f));
		f(std::get<idx>(tuple), idx);
	}
};

template<typename T, typename F>
struct Visitor<T, F, -1>{
	void visit(T&&, F&&){}
};

}

template<typename T, typename F>
void visit(T&& tuple, F&& f){
	impl::Visitor<T, F> visitor;
	visitor.visit(std::forward<T>(tuple), std::forward<F>(f));
}

}

namespace type{

template<typename T>
struct IsVector : std::false_type{};

template<typename... Args>
struct IsVector<std::vector<Args...>> : std::true_type{};

template<typename T>
struct IsOptional : std::false_type{};

template<typename... Args>
struct IsOptional<std::optional<Args...>> : std::true_type{};

template<typename T>
struct IsVariant : std::false_type{};

template<typename... Args>
struct IsVariant<std::variant<Args...>> : std::true_type{};

template<typename T>
struct IsTuple : std::false_type{};

template<typename... Args>
struct IsTuple<std::tuple<Args...>> : std::true_type{};

}
}
