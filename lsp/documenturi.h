#pragma once

#include <lsp/uri.h>

namespace lsp{

/*
 * DocumentUri
 */

class DocumentUri : public Uri{
public:
	static constexpr auto Scheme = std::string_view("file");

	DocumentUri();

	std::string_view path() const;
	bool setPath(std::string_view path);

	// Constructors required for StrMap. Prefer Uri::parse
	DocumentUri(const std::string& str){ *this = parse(str); }
	DocumentUri(std::string_view str){ *this = parse(str); }
	DocumentUri(const char* str){ *this = parse(str); }

	DocumentUri(DocumentUri&&) = default;
	DocumentUri(const DocumentUri&) = default;
	DocumentUri& operator=(DocumentUri&&) = default;
	DocumentUri& operator=(const DocumentUri&) = default;

	DocumentUri(Uri&& other);
	DocumentUri& operator=(Uri&& other);

private:
	using Uri::path;
	using Uri::setPath;
	using Uri::setScheme;
};

using FileURI [[deprecated]] = DocumentUri;

} // namespace lsp
