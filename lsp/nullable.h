#pragma once

#include <cstddef>
#include <optional>

namespace lsp{

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

	bool isNull() const{ return !m_value.has_value(); }
	T& value(){ return m_value.value(); }
	const T& value() const{ return m_value.value(); }

private:
	std::optional<T> m_value;
};

}
