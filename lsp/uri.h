#pragma once

#include <string>
#include <cstdint>

namespace lsp{

/*
 * Uri
 */

class Uri{
public:
	Uri() = default;

	static Uri parse(std::string_view uriStr);

	// Constructors required for StrMap. Prefer Uri::parse
	Uri(const std::string& str){ *this = parse(str); }
	Uri(std::string_view str){ *this = parse(str); }
	Uri(const char* str){ *this = parse(str); }

	std::string toString() const;

	bool isValid() const;
	bool hasAuthority() const;
	bool hasQuery() const;
	bool hasFragment() const;

	bool setScheme(std::string_view scheme);
	bool setAuthority(std::string_view authority);
	bool setPath(std::string_view path);
	bool setQuery(std::string_view query);
	bool setFragment(std::string_view fragment);

	std::string_view scheme() const;
	std::string_view authority() const;
	std::string_view path() const;
	std::string_view query() const;
	std::string_view fragment() const;

	static std::string encode(std::string_view decoded, std::string_view exclude = {});
	static std::string decode(std::string_view encoded);

	const std::string& data() const{ return m_data; }

	bool operator==(const Uri& other) const{ return m_data == other.m_data; }
	bool operator!=(const Uri& other) const{ return m_data != other.m_data; }
	bool operator==(std::string_view other) const{ return toString() == other; }
	bool operator!=(std::string_view other) const{ return toString() != other; }
	bool operator<(const Uri& other) const{ return m_data < other.m_data; }

private:
	std::string   m_data;
	std::uint16_t m_schemeLen    = 0;
	std::uint16_t m_authorityLen = 0;
	std::uint16_t m_pathLen      = 0;
	std::uint16_t m_queryLen     = 0;
	std::uint16_t m_fragmentLen  = 0;
	std::uint8_t  m_hasAuthority : 1 = 0;
	std::uint8_t  m_hasQuery     : 1 = 0;
	std::uint8_t  m_hasFragment  : 1 = 0;

	void insertScheme(std::string_view scheme);
	void insertAuthority(std::string_view authority);
	void insertPath(std::string_view path);
	void insertQuery(std::string_view query);
	void insertFragment(std::string_view fragment);
};

} // namespace lsp
