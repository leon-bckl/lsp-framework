#pragma once

#include <memory>
#include <cstddef>
#include <optional>
#include <concepts>

namespace lsp::util{

/*
 * Either a valid value or null.
 * Nullable values are always serialized to json unlike an optional which is ommitted if it does not have a value.
 */
template<typename T>
class Nullable{
public:
	Nullable() = default;

	Nullable(const T& t){
		*this = t;
	}

	Nullable(T&& t){
		*this = std::forward<T>(t);
	}

	Nullable& operator=(const T& t){
		m_value.emplace(t);
		return *this;
	}

	Nullable& operator=(T&& t){
		m_value.emplace(std::forward<T>(t));
		return *this;
	}

	Nullable& operator=(std::nullptr_t){
		m_value = std::nullopt;
		return *this;
	}

	template<typename... Args>
	T& emplace(Args&&... args){
		return m_value.emplace(std::forward<Args>(args)...);
	}

	bool isNull() const{ return !m_value.has_value(); }
	T& value(){ return m_value.value(); }
	const T& value() const{ return m_value.value(); }
	T* operator->(){ return std::addressof(value()); }
	const T* operator->() const{ return std::addressof(value()); }

private:
	std::optional<T> m_value;
};

} // namespace lsp::util
