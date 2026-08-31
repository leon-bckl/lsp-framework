#pragma once

#include <string>

namespace lsp{

template<typename ValueType>
struct EnumerationTypeHelper{
	using ConstInitType = ValueType;
};

template<>
struct EnumerationTypeHelper<std::string>{
	using ConstInitType = std::string_view;
};

template<typename EnumType, typename ValueT>
class Enumeration{
public:
	using ValueType     = ValueT;
	using ConstInitType = typename EnumerationTypeHelper<ValueType>::ConstInitType;

	Enumeration() = default;
	Enumeration(EnumType index) : m_index{index}{}
	Enumeration(ValueType&& value){ *this = std::move(value); }

	auto operator=(EnumType other) -> Enumeration&{ m_index = other; return *this; }

	auto operator=(ValueType&& other) -> Enumeration&
	{
		for(unsigned int i = 0; const auto& v : s_values)
		{
			if(v == other)
			{
				m_index = static_cast<EnumType>(i);

				return *this;
			}

			++i;
		}

		m_index       = EnumType::MAX_VALUE;
		m_customValue = std::forward<ValueType>(other);

		return *this;
	}

	auto operator==(EnumType other) const -> bool{ return m_index == other; }
	auto operator==(ConstInitType other) const -> bool{ return value() == other; }
	auto operator!=(EnumType other) const -> bool{ return m_index != other; }
	auto operator!=(ConstInitType other) const -> bool{ return value() != other; }
	operator ValueType() const{ return ValueType(value()); }
	operator EnumType() const{ return index(); }

	auto hasCustomValue() const -> bool{ return m_index == EnumType::MAX_VALUE; }
	auto index() const -> EnumType{ return m_index; }

	static auto value(EnumType index) -> ConstInitType
	{
		return s_values[static_cast<unsigned int>(index)];
	}

	auto value() const -> ConstInitType
	{
		if(hasCustomValue())
			return m_customValue;

		return value(m_index);
	}

private:
	EnumType  m_index       = EnumType::MAX_VALUE;
	ValueType m_customValue = {};

	static const ConstInitType s_values[static_cast<std::size_t>(EnumType::MAX_VALUE)];
};

template<typename EnumType, typename ValueType>
const typename EnumerationTypeHelper<ValueType>::ConstInitType
	Enumeration<EnumType, ValueType>::s_values[] = {};

} // namespace lsp
