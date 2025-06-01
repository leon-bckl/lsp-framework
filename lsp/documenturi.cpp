#include <filesystem>
#include <lsp/documenturi.h>

namespace lsp{

DocumentUri::DocumentUri()
{
	setScheme(Scheme);
	setAuthority({});
}

std::string_view DocumentUri::path() const
{
	const auto p = Uri::path();

#ifdef _WIN32
	if(p.starts_with('/'))
		return p.substr(1);
#endif

	return p;
}

bool DocumentUri::setPath(std::string_view path)
{
	const auto absolute = std::filesystem::absolute(path);
#ifdef _WIN32
	Uri::setPath('/' + absolute.string());
#else
	Uri::setPath(absolute .string());
#endif
	return true;
}

DocumentUri::DocumentUri(Uri&& other)
	: Uri(other.isValid() && other.scheme() == Scheme ? std::move(other) : Uri())
{
}

DocumentUri& DocumentUri::operator=(Uri&& other)
{
	*this = DocumentUri(std::move(other));
	return *this;
}

} // namespace lsp
