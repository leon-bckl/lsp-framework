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

template<typename EnumType, typename ValueType>
class Enumeration{
	using ConstInitType = typename EnumerationTypeHelper<ValueType>::ConstInitType;
public:
	Enumeration() = default;
	Enumeration(EnumType index) : m_index{index}{}
	explicit Enumeration(ValueType&& value){ *this = value; }

	Enumeration& operator=(EnumType other){ m_index = other; return *this; }

	Enumeration& operator=(ValueType&& other)
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

	bool operator==(EnumType other) const{ return m_index == other; }
	bool operator==(ConstInitType other) const{ return value() == other; }
	operator ValueType() const{ return value(); }
	operator EnumType() const{ return index(); }
	bool hasCustomValue() const{ return m_index == EnumType::MAX_VALUE; }
	EnumType index() const{ return m_index; }

	static ConstInitType value(EnumType index)
	{
		return s_values[static_cast<unsigned int>(index)];
	}

	ConstInitType value() const
	{
		if(hasCustomValue())
			return m_customValue;

		return value(m_index);
	}

private:
	EnumType  m_index       = EnumType::MAX_VALUE;
	ValueType m_customValue = {};

	static const ConstInitType s_values[static_cast<int>(EnumType::MAX_VALUE)];
};

template<typename EnumType, typename ValueType>
const typename EnumerationTypeHelper<ValueType>::ConstInitType
	Enumeration<EnumType, ValueType>::s_values[] = {};

} // namespace lsp
