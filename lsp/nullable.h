#pragma once

#include <cstddef>
#include <utility>
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
	using value_type = T;

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

	[[nodiscard]] bool isNull() const{ return !m_value.has_value(); }
	[[nodiscard]] T& value(){ return m_value.value(); }
	[[nodiscard]] const T& value() const{ return m_value.value(); }
	[[nodiscard]] T& operator*(){ return *m_value; }
	[[nodiscard]] const T& operator*() const{ return *m_value; }
	[[nodiscard]] T* operator->(){ return std::addressof(value()); }
	[[nodiscard]] const T* operator->() const{ return std::addressof(value()); }

private:
	std::optional<T> m_value;
};

template<typename... Args>
class NullableVariant{
public:
	using VariantType = std::variant<Args...>;
	using value_type  = VariantType;

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
	[[nodiscard]] bool holdsAlternative()
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
		m_value.emplace(std::in_place_type<T>, std::forward<Params>(params)...);
		return get<T>();
	}

	void reset(){ m_value.reset(); }
	[[nodiscard]] bool isNull() const{ return !m_value.has_value(); }

	template<typename T>
	[[nodiscard]] T& get(){ return std::get<T>(*m_value); }
	template<typename T>
	[[nodiscard]] const T& get() const{ return std::get<T>(*m_value); }

	[[nodiscard]] VariantType& value(){ return *m_value; }
	[[nodiscard]] const VariantType& value() const{ return *m_value; }
	[[nodiscard]] VariantType& operator*(){ return *m_value; }
	[[nodiscard]] const VariantType& operator*() const{ return *m_value; }

private:
	std::optional<VariantType> m_value;
};

} // namespace lsp
