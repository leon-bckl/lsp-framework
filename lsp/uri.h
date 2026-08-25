#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace lsp{

class Uri{
public:
	static constexpr auto FileScheme = std::string_view("file");

	Uri() = default;

	[[nodiscard]] static Uri parse(std::string_view uriStr);
	[[nodiscard]] static Uri fileUriFromPath(std::string_view path);

	[[nodiscard]] std::string fsPath() const;

	[[nodiscard]] bool isValid() const;
	[[nodiscard]] bool isFileUri() const;
	[[nodiscard]] bool hasAuthority() const;
	[[nodiscard]] bool hasQuery() const;
	[[nodiscard]] bool hasFragment() const;

	[[nodiscard]] std::string_view scheme() const;
	[[nodiscard]] std::string_view authority() const;
	[[nodiscard]] std::string_view path() const;
	[[nodiscard]] std::string_view query() const;
	[[nodiscard]] std::string_view fragment() const;

	bool setScheme(std::string_view scheme);
	bool setAuthority(std::string_view authority);
	bool setPath(std::string_view path);
	bool setQuery(std::string_view query);
	bool setFragment(std::string_view fragment);

	void removeAuthority();
	void removeQuery();
	void removeFragment();

	[[nodiscard]] std::string toString() const;
	[[nodiscard]] std::string_view data() const{ return m_data; }

	[[nodiscard]] static std::string encode(std::string_view decoded, std::string_view exclude = {});
	[[nodiscard]] static std::string decode(std::string_view encoded);

	[[nodiscard]] bool operator==(const Uri& other) const;
	[[nodiscard]] bool operator!=(const Uri& other) const{ return !(*this == other); }
	[[nodiscard]] bool operator<(const Uri& other) const{ return m_data < other.m_data; }

private:
	std::string   m_data;
	std::uint16_t m_schemeLen        = 0;
	std::uint16_t m_authorityLen     = 0;
	std::uint16_t m_pathLen          = 0;
	std::uint16_t m_queryLen         = 0;
	std::uint16_t m_fragmentLen      = 0;
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

namespace std{

template<>
struct hash<lsp::Uri>{
	using is_transparent = void;

	size_t operator()(const lsp::Uri& uri) const{ return hash<string_view>{}(uri.data()); }
	size_t operator()(string_view uriStr) const{ return hash<string_view>{}(lsp::Uri::parse(uriStr).data()); }
};

template<>
struct equal_to<lsp::Uri>{
	using is_transparent = void;

	bool operator()(const lsp::Uri& lhs, const lsp::Uri& rhs) const{ return lhs == rhs; }
	bool operator()(const lsp::Uri& lhs, string_view rhs) const{ return lhs == lsp::Uri::parse(rhs); }
};

} // namespace std
