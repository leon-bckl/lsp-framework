#include "name.h"
#include <string>
#include <cctype>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <string_view>
#include <unordered_map>
#include "names.h"

namespace uscript{
namespace{

struct NameEntry{
	std::string_view name;
	int              flags;
};

struct CaseInsensitiveStringHash{
	std::size_t operator()(std::string_view str) const{
		std::vector<char> upper(str.size());
		std::transform(str.cbegin(), str.cend(), upper.begin(), [](char c){return std::toupper(c);});
		return std::hash<std::string_view>{}({upper.data(), upper.size()});
	}
};

struct CaseInsensitiveStringEqual{
	bool operator()(const std::string& s1, const std::string& s2) const{
		return s1.size() == s2.size() &&
#ifdef _MSC_VER
		strnicmp(s1.data(), s2.data(), s1.size()) == 0;
#else
		strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
#endif
	}
};

/*
 * Stores the actual string data for a name and its index into the name table.
 */
std::unordered_map<std::string, Name::index_type, CaseInsensitiveStringHash, CaseInsensitiveStringEqual> g_nameIndexLookup;

/*
 * Stores all existing names so they can easily be referenced by their index.
 * The entries have a string view that references the data from the key in the index lookup.
 */
std::vector<NameEntry> g_nameTable;

Name::index_type createName(std::string_view name, int flags = 0){
	auto index = static_cast<Name::index_type>(g_nameTable.size());
	auto inserted = g_nameIndexLookup.insert(std::make_pair(std::string{name.cbegin(), name.cend()}, index));
	assert(inserted.second);
	g_nameTable.emplace_back(NameEntry{inserted.first->first, flags});

	return index;
}

}

Name::Name(std::string_view name){
	m_index = findIndex(name);

	if(m_index == InvalidIndex)
		m_index = createName(name);
}

Name::Name(index_type index) : m_index{index}{
	if(m_index < 0 || static_cast<std::size_t>(m_index) >= g_nameTable.size())
		m_index = 0;
}

int Name::flags() const{
	return g_nameTable[m_index].flags;
}

std::string_view Name::toString() const{
	return g_nameTable[m_index].name;
}

Name::index_type Name::findIndex(std::string_view name){
	if(!name.empty()){
		auto it = g_nameIndexLookup.find(std::string{name.cbegin(), name.cend()});

		if(it != g_nameIndexLookup.end())
			return it->second;
	}

	return InvalidIndex;
}

void Name::initialize(){
#define REGISTER_NAME(name) createName(#name);
#define REG_NAME_HIGH(name) createName(#name, Name::Highlighted);
#include "names.h"
	assert(Name::findIndex("None") == NAME_None);
}

}
