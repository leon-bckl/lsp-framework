#pragma once

#include <lsp/uri.h>

namespace lsp{

/*
 * DocumentUri
 */

class FileUri : public Uri{
public:
	static constexpr auto Scheme = std::string_view("file");

	FileUri();

	[[nodiscard]] static FileUri fromPath(std::string_view path);

	std::string_view path() const;
	bool setPath(std::string_view path);

	// Constructors required for StrMap. Prefer Uri::parse
	FileUri(const std::string& str){ *this = parse(str); }
	FileUri(std::string_view str){ *this = parse(str); }
	FileUri(const char* str){ *this = parse(str); }

	FileUri(FileUri&&) = default;
	FileUri(const FileUri&) = default;
	FileUri& operator=(FileUri&&) = default;
	FileUri& operator=(const FileUri&) = default;

	FileUri(Uri&& other);
	FileUri& operator=(Uri&& other);

private:
	using Uri::path;
	using Uri::setPath;
	using Uri::setScheme;
};

using DocumentUri = FileUri;

using FileURI [[deprecated]] = FileUri;

} // namespace lsp
