#pragma once

#include <type_traits>

namespace lsp::util{

template<typename E>
class EnumFlags{
public:
	using UnderlyingType = std::underlying_type<E>::type;

	constexpr EnumFlags() = default;
	constexpr EnumFlags(E e) : m_flags{static_cast<UnderlyingType>(e)}{}
	constexpr EnumFlags(UnderlyingType flags) : m_flags{flags}{}

	constexpr bool operator==(EnumFlags other) const{ return m_flags == other.m_flags; }
	constexpr bool operator!=(EnumFlags other) const{ return m_flags != other.m_flags; }
	constexpr EnumFlags operator&(EnumFlags other) const{ return m_flags & other.m_flags; }
	constexpr EnumFlags operator|(EnumFlags other) const{ return m_flags | other.m_flags; }
	constexpr EnumFlags operator^(EnumFlags other) const{ return m_flags ^ other.m_flags; }
	constexpr EnumFlags operator~() const{ return ~m_flags; }
	constexpr void operator&=(EnumFlags other) const{ m_flags &= other.m_flags; }
	constexpr void operator|=(EnumFlags other) const{ m_flags |= other.m_flags; }
	constexpr void operator^=(EnumFlags other) const{ m_flags ^= other.m_flags; }

private:
	UnderlyingType m_flags = 0;
};

#define LSP_DECLARE_ENUM_FLAGS(enumName, typedefName) \
	using typedefName = lsp::util::EnumFlags<enumName>; \
	inline constexpr typedefName operator&(enumName e1, enumName e2){ return typedefName{e1} & e2; } \
	inline constexpr typedefName operator|(enumName e1, enumName e2){ return typedefName{e1} | e2; } \
	inline constexpr typedefName operator^(enumName e1, enumName e2){ return typedefName{e1} ^ e2; } \
	inline constexpr typedefName operator~(enumName e1){ return ~typedefName{e1}; }

#define BIT(x) (1 << (x))

} // namespace lsp::util
