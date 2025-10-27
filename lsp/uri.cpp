#include <cctype>
#include <cassert>
#include <charconv>
#include <lsp/uri.h>

namespace lsp{
namespace{

std::uint16_t parseUriScheme(std::string_view uriStr)
{
	std::uint16_t len = 0;

	for(const char c : uriStr)
	{
		if(!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '.' && c != '+')
			break;

		++len;
	}

	return len;
}

std::uint16_t parseUriAuthority(std::string_view uriAuthorityStr)
{
	std::uint16_t len = 0;

	for(const char c : uriAuthorityStr)
	{
		if(c == '/' || c == '?' || c == '#')
			break;

		++len;
	}

	return len;
}

std::uint16_t parseUriPath(std::string_view uriPathStr)
{
	std::uint16_t len = 0;

	for(const char c : uriPathStr)
	{
		if(c == '?' || c == '#')
			break;

		++len;
	}

	return len;
}

std::uint16_t parseUriQuery(std::string_view uriQueryStr)
{
	std::uint16_t len = 0;

	for(const char c : uriQueryStr)
	{
		if(c == '#')
			break;

		++len;
	}

	return len;
}

bool hasCharAt(std::string_view str, std::size_t idx, char c)
{
	return str.size() > idx && str[idx] == c;
}

void normalizeEncodedCase(std::string& str, std::size_t first, std::size_t count)
{
	const auto end = std::min(first + count, str.size());

	for(std::size_t i = first; i < end; ++i)
	{
		if(str[i] == '%' && i + 2 < end)
		{
			auto j = i + 1;
			str[j] = static_cast<char>(std::toupper(static_cast<unsigned char>(str[j])));
			++j;
			str[j] = static_cast<char>(std::toupper(static_cast<unsigned char>(str[j])));

			i += 2;
		}
	}
}

} // namespace

Uri Uri::parse(std::string_view uriStr)
{
	auto uri = Uri();

	const auto schemeLen = static_cast<std::size_t>(parseUriScheme(uriStr));

	if(schemeLen == 0 || !hasCharAt(uriStr, schemeLen, ':'))
		return {};

	const auto scheme = uriStr.substr(0, schemeLen);
	auto       idx    = schemeLen;

	uri.insertScheme(scheme);
	idx += 1; // :

	const auto hasAuthority = hasCharAt(uriStr, idx, '/') && hasCharAt(uriStr, idx + 1, '/');

	if(hasAuthority)
	{
		idx += 2; // //
		const auto authorityLen = parseUriAuthority(uriStr.substr(idx));
		const auto authority    = uriStr.substr(idx, authorityLen);
		uri.insertAuthority(authority);
		idx += authorityLen;
	}

	const auto pathIsEmpty = idx >= uriStr.size();

	if(!pathIsEmpty)
	{
		if(hasAuthority && uriStr[idx] != '/')
			return {};

		const auto pathLen     = parseUriPath(uriStr.substr(idx));
		const auto path        = uriStr.substr(idx, pathLen);
		const auto decodedPath = decode(path);
		uri.insertPath(decodedPath);
		idx += pathLen;
	}

	const auto hasQuery = hasCharAt(uriStr, idx, '?');

	if(hasQuery)
	{
		idx += 1; // ?
		const auto queryLen = parseUriQuery(uriStr.substr(idx));
		const auto query    = uriStr.substr(idx, queryLen);
		uri.insertQuery(query);
		idx += queryLen;
	}

	const auto hasFragment = hasCharAt(uriStr, idx, '#');

	if(hasFragment)
	{
		idx += 1; // #
		const auto fragment = uriStr.substr(idx);
		uri.insertFragment(fragment);
		idx += fragment.size();
	}

	assert(idx == uriStr.size());

	return uri;
}

std::string Uri::toString() const
{
	if(!isValid())
		return {};

	const auto encodedPath = encode(path(), "/");

	auto result = std::string();
	result.reserve(m_data.size() - path().size() + encodedPath.size() + 5); // ://?#

	result += scheme();
	result += ':';

	if(hasAuthority())
	{
		result += "//";
		result += authority();
	}

	result += encodedPath;

	if(hasQuery())
	{
		result += '?';
		result += query();
	}

	if(hasFragment())
	{
		result += '#';
		result += fragment();
	}

	return result;
}

bool Uri::isValid() const
{
	return m_schemeLen > 0;
}

bool Uri::hasAuthority() const
{
	return !!m_hasAuthority;
}

bool Uri::hasQuery() const
{
	return !!m_hasQuery;
}

bool Uri::hasFragment() const
{
	return !!m_hasFragment;
}

std::string_view Uri::scheme() const
{
	return std::string_view(m_data).substr(0, m_schemeLen);
}

std::string_view Uri::authority() const
{
	if(!hasAuthority())
		return {};

	const auto authorityIdx = m_schemeLen;
	return std::string_view(m_data).substr(authorityIdx, m_authorityLen);
}

std::string_view Uri::path() const
{
	const auto pathIdx = std::size_t(m_schemeLen + m_authorityLen);
	return std::string_view(m_data).substr(pathIdx, m_pathLen);
}

std::string_view Uri::query() const
{
	if(!hasQuery())
		return {};

	const auto queryIdx = std::size_t(m_schemeLen + m_authorityLen + m_pathLen);
	return std::string_view(m_data).substr(queryIdx, m_queryLen);
}

std::string_view Uri::fragment() const
{
	if(!hasFragment())
		return {};

	const auto fragmentIdx = std::size_t(m_schemeLen + m_authorityLen + m_pathLen + m_queryLen);
	return std::string_view(m_data).substr(fragmentIdx, m_fragmentLen);
}

bool Uri::setScheme(std::string_view scheme)
{
	const auto len = parseUriScheme(scheme);

	if(len == scheme.size())
	{
		insertScheme(scheme);
		return true;
	}

	return false;
}

bool Uri::setAuthority(std::string_view authority)
{
	const auto len = parseUriAuthority(authority);

	if(len == authority.size())
	{
		insertAuthority(authority);
		return true;
	}

	return false;
}

bool Uri::setPath(std::string_view path)
{
	insertPath(path);
	return true;
}

bool Uri::setQuery(std::string_view query)
{
	const auto len = parseUriQuery(query);

	if(len == query.size())
	{
		insertQuery(query);
		return true;
	}

	return false;
}

bool Uri::setFragment(std::string_view fragment)
{
	insertFragment(fragment);
	return true;
}

void Uri::removeAuthority()
{
	insertAuthority({});
	m_hasAuthority = 0;
}

void Uri::removeQuery()
{
	insertQuery({});
	m_hasQuery = 0;
}

void Uri::removeFragment()
{
	insertFragment({});
	m_hasFragment = 0;
}

void Uri::insertScheme(std::string_view scheme)
{
	m_data.replace(0, m_schemeLen, scheme);
	m_schemeLen = static_cast<std::uint16_t>(scheme.size());

	for(std::uint16_t i = 0; i < m_schemeLen; ++i)
		m_data[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(m_data[i])));
}

void Uri::insertAuthority(std::string_view authority)
{
	const auto authorityIdx = m_schemeLen;
	m_data.replace(authorityIdx, m_authorityLen, authority);
	m_authorityLen = static_cast<std::uint16_t>(authority.size());
	normalizeEncodedCase(m_data, m_schemeLen, m_authorityLen);
	m_hasAuthority = 1;
}

void Uri::insertPath(std::string_view path)
{
	const auto pathIdx = std::size_t(m_schemeLen + m_authorityLen);
	m_data.replace(pathIdx, m_pathLen, path);
	m_pathLen = static_cast<std::uint16_t>(path.size());
}

void Uri::insertQuery(std::string_view query)
{
	const auto queryIdx = std::size_t(m_schemeLen + m_authorityLen + m_pathLen);
	m_data.replace(queryIdx, m_queryLen, query);
	m_queryLen = static_cast<std::uint16_t>(query.size());
	normalizeEncodedCase(m_data, queryIdx, m_queryLen);
	m_hasQuery = 1;
}

void Uri::insertFragment(std::string_view fragment)
{
	const auto fragmentIdx = std::size_t(m_schemeLen + m_authorityLen + m_pathLen + m_queryLen);
	m_data.replace(fragmentIdx, m_fragmentLen, fragment);
	m_fragmentLen = static_cast<std::uint16_t>(fragment.size());
	normalizeEncodedCase(m_data, fragmentIdx, m_fragmentLen);
	m_hasFragment = 1;
}

std::string Uri::encode(std::string_view decoded, std::string_view exclude)
{
	std::string encoded;
	encoded.reserve(decoded.size());

	for(const char c : decoded)
	{
		if(exclude.find(c) != std::string_view::npos ||
		   std::isalnum(static_cast<unsigned char>(c)) ||
		   c == '_' ||
		   c == '.' ||
		   c == '-')
		{
			encoded += c;
		}
		else
		{
			constexpr auto hexLookup = "0123456789ABCDEF";
			encoded += '%';
			encoded += hexLookup[c >> 4];
			encoded += hexLookup[c & 0xF];
		}
	}

	return encoded;
}

std::string Uri::decode(std::string_view encoded)
{
	std::string decoded;

	for(std::size_t i = 0; i < encoded.size(); ++i)
	{
		if(encoded[i] == '%' && i + 2 < encoded.size())
		{
			const char* start = &encoded[i + 1];
			const char* end   = &encoded[i + 3];

			unsigned char c;
			const auto [ptr, ec] = std::from_chars(start, end, c, 16);

			if(ec != std::errc{} || ptr != end)
				return {};

			decoded += static_cast<char>(c);
			i += 2;
		}
		else
		{
			decoded += encoded[i];
		}
	}

	return decoded;
}

} // namespace lsp
