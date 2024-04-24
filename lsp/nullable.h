#pragma once

#include <cstddef>
#include <variant>
#include <optional>

namespace lsp{

/*
 * Either a valid value or null.
 * Nullable values are always serialized to json unlike an optional which is ommitted if it does not have a value.
 */
template<typename T>
class Nullable{
public:
	Nullable() = default;
	Nullable(std::nullptr_t) : Nullable{}{}
	Nullable(const T& t){ *this = t; }
	Nullable(T&& t){ *this = std::forward<T>(t); }

	Nullable& operator=(const T& t)
	{
		m_value.emplace(t);
		return *this;
	}

	Nullable& operator=(T&& t)
	{
		m_value.emplace(std::forward<T>(t));
		return *this;
	}

	Nullable& operator=(std::nullptr_t)
	{
		m_value = std::nullopt;
		return *this;
	}

	template<typename... Args>
	T& emplace(Args&&... args)
	{
		return m_value.emplace(std::forward<Args>(args)...);
	}

	void reset(){ m_value.reset(); }
	bool isNull() const{ return !m_value.has_value(); }
	T& value(){ return m_value.value(); }
	const T& value() const{ return m_value.value(); }
	T& operator*(){ return *m_value; }
	const T& operator*() const{ return *m_value; }
	T* operator->(){ return std::addressof(value()); }
	const T* operator->() const{ return std::addressof(value()); }

private:
	std::optional<T> m_value;
};

template<typename... Args>
class NullableVariant{
public:
	using VariantType = std::variant<Args...>;

	NullableVariant() = default;
	NullableVariant(std::nullptr_t) : NullableVariant{}{}

	template<typename T>
	NullableVariant(const T& t)
	{
		*this = t;
	}

	template<typename T>
	NullableVariant& operator=(const T& t)
	{
		m_value.emplace(t);
		return *this;
	}

	template<typename T>
	NullableVariant& operator=(T&& t)
	{
		m_value.emplace(std::forward<T>(t));
		return *this;
	}

	NullableVariant& operator=(std::nullptr_t)
	{
		m_value = std::nullopt;
		return *this;
	}

	template<typename T>
	bool holdsAlternative()
	{
		return !isNull() && std::holds_alternative<T>(value());
	}

	void emplace(const VariantType& variant)
	{
		m_value.emplace(variant);
	}

	void emplace(VariantType&& variant)
	{
		m_value.emplace(std::move(variant));
	}

	template<typename T, typename... Params>
	T& emplace(Params&&... params)
	{
		return m_value.emplace(std::forward<Params>(params)...);
	}

	void reset(){ m_value.reset(); }
	bool isNull() const{ return !m_value.has_value(); }

	template<typename T>
	T& get(){ return std::get<T>(*m_value); }
	template<typename T>
	const T& get() const{ return std::get<T>(*m_value); }

	VariantType& value(){ return *m_value; }
	const VariantType& value() const{ return *m_value; }
	VariantType& operator*(){ return *m_value; }
	const VariantType& operator*() const{ return *m_value; }

private:
	std::optional<VariantType> m_value;
};

} // namespace lsp
