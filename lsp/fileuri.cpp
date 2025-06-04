#include <filesystem>
#include <lsp/fileuri.h>

namespace lsp{

FileUri::FileUri()
{
	setScheme(Scheme);
	setAuthority({});
}

FileUri FileUri::fromPath(std::string_view path)
{
	auto uri = FileUri();
	uri.setPath(path);
	return uri;
}

std::string_view FileUri::path() const
{
	const auto p = Uri::path();
#ifdef _WIN32
	if(p.starts_with('/'))
		return p.substr(1);
#endif
	return p;
}

bool FileUri::setPath(std::string_view path)
{
	const auto absolute = std::filesystem::absolute(path);
#ifdef _WIN32
	Uri::setPath('/' + absolute.string());
#else
	Uri::setPath(absolute .string());
#endif
	return true;
}

FileUri::FileUri(const Uri& other)
	: Uri(other.isValid() && other.scheme() == Scheme ? std::move(other) : Uri())
{
}

FileUri& FileUri::operator=(const Uri& other)
{
	*this = FileUri(other);
	return *this;
}

FileUri::FileUri(Uri&& other)
	: Uri(other.isValid() && other.scheme() == Scheme ? std::move(other) : Uri())
{
}

FileUri& FileUri::operator=(Uri&& other)
{
	*this = FileUri(std::move(other));
	return *this;
}

} // namespace lsp
