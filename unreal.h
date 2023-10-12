#pragma once

#include <string_view>
#include "unrealNames.h"

namespace unreal{

void initialize();

class Name{
public:
	using index_type = int;
	static constexpr index_type InvalidIndex = -1;

	enum{
		Highlighted
	};

	Name() = default;
	Name(index_type index);
	Name(std::string_view name);

	[[nodiscard]] bool operator==(Name other) const{ return m_index == other.m_index; }
	[[nodiscard]] bool operator!=(Name other) const{ return m_index != other.m_index; }
	[[nodiscard]] bool operator==(index_type index) const{ return m_index == index; }
	[[nodiscard]] bool operator!=(index_type index) const{ return m_index != index; }

	[[nodiscard]] int flags() const;
	[[nodiscard]] std::string_view toString() const;

	[[nodiscard]] static index_type findIndex(std::string_view name);

private:
	index_type m_index = 0;
};

}
