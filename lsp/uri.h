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

	[[nodiscard]] static auto parse(std::string_view uriStr) -> Uri;
	[[nodiscard]] static auto fileUriFromPath(std::string_view path) -> Uri;

	[[nodiscard]] std::string fsPath() const;

	[[nodiscard]] auto isValid() const -> bool;
	[[nodiscard]] auto isFileUri() const -> bool;
	[[nodiscard]] auto hasAuthority() const -> bool;
	[[nodiscard]] auto hasQuery() const -> bool;
	[[nodiscard]] auto hasFragment() const -> bool;

	[[nodiscard]] auto scheme() const -> std::string_view;
	[[nodiscard]] auto authority() const -> std::string_view;
	[[nodiscard]] auto path() const -> std::string_view;
	[[nodiscard]] auto query() const -> std::string_view;
	[[nodiscard]] auto fragment() const -> std::string_view;

	auto setScheme(std::string_view scheme) -> bool;
	auto setAuthority(std::string_view authority) -> bool;
	auto setPath(std::string_view path) -> bool;
	auto setQuery(std::string_view query) -> bool;
	auto setFragment(std::string_view fragment) -> bool;

	void removeAuthority();
	void removeQuery();
	void removeFragment();

	[[nodiscard]] auto toString() const -> std::string;
	[[nodiscard]] auto data() const -> std::string_view{ return m_data; }

	[[nodiscard]] static auto encode(std::string_view decoded, std::string_view exclude = {}) -> std::string;
	[[nodiscard]] static auto decode(std::string_view encoded) -> std::string;

	[[nodiscard]] auto operator==(const Uri& other) const -> bool;
	[[nodiscard]] auto operator!=(const Uri& other) const -> bool{ return !(*this == other); }
	[[nodiscard]] auto operator<(const Uri& other) const -> bool{ return m_data < other.m_data; }

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

	auto operator()(const lsp::Uri& uri) const -> size_t{ return hash<string_view>{}(uri.data()); }
	auto operator()(string_view uriStr) const -> size_t{ return hash<string_view>{}(lsp::Uri::parse(uriStr).data()); }
};

template<>
struct equal_to<lsp::Uri>{
	using is_transparent = void;

	auto operator()(const lsp::Uri& lhs, const lsp::Uri& rhs) const -> bool{ return lhs == rhs; }
	auto operator()(const lsp::Uri& lhs, string_view rhs) const -> bool{ return lhs == lsp::Uri::parse(rhs); }
};

} // namespace std
