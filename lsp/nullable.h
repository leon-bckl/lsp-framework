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
	Nullable(std::nullptr_t) : Nullable(){}
	Nullable(const T& t){ *this = t; }
	Nullable(T&& t){ *this = std::forward<T>(t); }

	auto operator=(const T& t) -> Nullable&
	{
		m_value.emplace(t);
		return *this;
	}

	auto operator=(T&& t) -> Nullable&
	{
		m_value.emplace(std::forward<T>(t));
		return *this;
	}

	auto operator=(std::nullptr_t) -> Nullable&
	{
		m_value = std::nullopt;
		return *this;
	}

	template<typename... Args>
	auto emplace(Args&&... args) -> T&
	{
		return m_value.emplace(std::forward<Args>(args)...);
	}

	void reset(){ m_value.reset(); }

	[[nodiscard]] auto isNull() const -> bool{ return !m_value.has_value(); }
	[[nodiscard]] auto value() -> T&{ return m_value.value(); }
	[[nodiscard]] auto value() const -> const T&{ return m_value.value(); }
	[[nodiscard]] auto operator*() -> T&{ return *m_value; }
	[[nodiscard]] auto operator*() const -> const T&{ return *m_value; }
	[[nodiscard]] auto operator->() -> T*{ return std::addressof(value()); }
	[[nodiscard]] auto operator->() const -> const T*{ return std::addressof(value()); }

private:
	std::optional<T> m_value;
};

template<typename... Args>
class NullableVariant{
public:
	using VariantType = std::variant<Args...>;
	using value_type  = VariantType;

	NullableVariant() = default;
	NullableVariant(std::nullptr_t) : NullableVariant(){}

	template<typename T>
	NullableVariant(const T& t)
	{
		*this = t;
	}

	template<typename T>
	auto operator=(const T& t) -> NullableVariant&
	{
		m_value.emplace(t);
		return *this;
	}

	template<typename T>
	auto operator=(T&& t) -> NullableVariant&
	{
		m_value.emplace(std::forward<T>(t));
		return *this;
	}

	auto operator=(std::nullptr_t) -> NullableVariant&
	{
		m_value = std::nullopt;
		return *this;
	}

	template<typename T>
	[[nodiscard]] auto holdsAlternative() -> bool
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
	auto emplace(Params&&... params) -> T&
	{
		m_value.emplace(std::in_place_type<T>, std::forward<Params>(params)...);
		return get<T>();
	}

	void reset(){ m_value.reset(); }

	[[nodiscard]] auto isNull() const -> bool{ return !m_value.has_value(); }

	template<typename T>
	[[nodiscard]] auto get() -> T&{ return std::get<T>(*m_value); }
	template<typename T>
	[[nodiscard]] auto get() const -> const T&{ return std::get<T>(*m_value); }

	[[nodiscard]] auto value() -> VariantType&{ return *m_value; }
	[[nodiscard]] auto value() const -> const VariantType&{ return *m_value; }
	[[nodiscard]] auto operator*() -> VariantType&{ return *m_value; }
	[[nodiscard]] auto operator*() const -> const VariantType&{ return *m_value; }

private:
	std::optional<VariantType> m_value;
};

} // namespace lsp
