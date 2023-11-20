#pragma once

#include <tuple>
#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace lsp::util{
namespace tuple{
namespace impl{

template<typename T, typename F, int idx = std::tuple_size_v<typename std::decay<T>::type> - 1>
struct Visitor
{
	void visit(T&& tuple, F&& f)
	{
		Visitor<T, F, idx - 1> visitor;
		visitor.visit(std::forward<T>(tuple), std::forward<F>(f));
		f(std::get<idx>(tuple), idx);
	}
};

template<typename T, typename F>
struct Visitor<T, F, -1>
{
	void visit(T&&, F&&){}
};

} // namespace impl

template<typename T, typename F>
void visit(T&& tuple, F&& f)
{
	impl::Visitor<T, F> visitor;
	visitor.visit(std::forward<T>(tuple), std::forward<F>(f));
}

} // namespace tuple

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

} // namespace type
} // namespace lsp::util
