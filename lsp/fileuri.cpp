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
	const auto absolute = std::filesystem::absolute(std::u8string_view((const char8_t*)(path.data()), path.size()));
#ifdef _WIN32
	const auto u8Path = u8'/' + absolute.u8string();
#else
	const auto u8Path = absolute.u8string();
#endif
	return Uri::setPath(std::string_view((const char*)u8Path.data(), u8Path.size()));
}

FileUri::FileUri(const Uri& other)
	: Uri(other.isValid() && other.scheme() == Scheme ? other : Uri())
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
