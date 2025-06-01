#include <cassert>
#include <charconv>
#include <algorithm>
#include <lsp/uri.h>

namespace lsp{
namespace{

std::uint16_t parseUriScheme(std::string_view uriStr)
{
	std::uint16_t len = 0;

	for(const unsigned char c : uriStr)
	{
		if(!std::isalnum(c) && c != '-' && c != '.' && c != '+')
			break;

		++len;
	}

	return len;
}

std::uint16_t parseUriAuthority(std::string_view uriAuthorityStr)
{
	std::uint16_t len = 0;

	for(const unsigned char c : uriAuthorityStr)
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

	for(const unsigned char c : uriPathStr)
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

	for(const unsigned char c : uriQueryStr)
	{
		if(c == '#')
			break;

		++len;
	}

	return len;
}

void replaceStringRange(std::string& str, std::size_t first, std::size_t len, std::string_view replacement)
{
	assert(first + len <= str.size());

	const auto replacementLen = replacement.size();

	if(len < replacementLen)
	{
		const auto sizeDiff = replacementLen - len;
		const auto copySize = str.size() - (first + len);
		str.resize(str.size() + sizeDiff);
		std::copy_n(str.begin() + first + len,
		            copySize,
		            str.begin() + first + replacementLen);
	}
	else if(len > replacementLen)
	{
		const auto sizeDiff = len - replacementLen;
		std::copy_backward(str.begin() + first + len,
		                   str.end(),
		                   str.end() - sizeDiff);
		str.resize(str.size() - sizeDiff);
	}

	std::copy_n(replacement.begin(), replacementLen, str.begin() + first);
}

} // namespace

Uri Uri::parse(std::string_view uriStr)
{
	auto uri = Uri();

	const auto schemeLen = parseUriScheme(uriStr);
	const auto scheme    = uriStr.substr(0, schemeLen);


	if(schemeLen == 0 || schemeLen == uriStr.size() || uriStr[schemeLen] != ':')
		return {};

	uri.insertScheme(scheme);

	std::uint16_t nextComponentStart = schemeLen + 1;
	const auto hasAuthority = uriStr.size() >= nextComponentStart + 2 &&
														uriStr[nextComponentStart] == '/' &&
														uriStr[nextComponentStart + 1] == '/';

	if(hasAuthority)
	{
		nextComponentStart += 2;
		const auto authorityLen = parseUriAuthority(uriStr.substr(nextComponentStart));
		const auto authority    = uriStr.substr(nextComponentStart, authorityLen);
		uri.insertAuthority(authority);
		nextComponentStart += authorityLen;
	}

	if(nextComponentStart < uriStr.size())
	{
		if(hasAuthority && uriStr[nextComponentStart] != '/')
			return {};

		const auto pathLen = parseUriPath(uriStr.substr(nextComponentStart));
		const auto path    = uriStr.substr(nextComponentStart, pathLen);
		uri.insertPath(decode(path));
		nextComponentStart += pathLen;
	}

	const auto hasQuery = uriStr.size() >= nextComponentStart + 1 && uriStr[nextComponentStart] == '?';

	if(hasQuery)
	{
		nextComponentStart += 1;
		const auto queryLen = parseUriQuery(uriStr.substr(nextComponentStart));
		const auto query    = uriStr.substr(nextComponentStart, queryLen);
		uri.insertQuery(query);
		nextComponentStart += queryLen;
	}

	const auto hasFragment = uriStr.size() >= nextComponentStart + 1 && uriStr[nextComponentStart] == '#';

	if(hasFragment)
	{
		nextComponentStart += 1;
		const auto fragment = uriStr.substr(nextComponentStart);
		uri.insertFragment(fragment);
	}

	return uri;
}

std::string Uri::toString() const
{
	if(!isValid())
		return {};

	auto result = std::string();
	result.reserve(m_data.size() + 5); // ://?#

	result += scheme();
	result += ':';

	if(hasAuthority())
	{
		result += "//";
		result += authority();
	}

	result += encode(path(), "/");

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

std::string_view Uri::scheme() const
{
	return std::string_view(m_data).substr(0, m_schemeLen);
}

std::string_view Uri::authority() const
{
	if(!hasAuthority())
		return {};

	return std::string_view(m_data).substr(m_schemeLen, m_authorityLen);
}

std::string_view Uri::path() const
{
	return std::string_view(m_data).substr(m_schemeLen + m_authorityLen,
	                                       m_pathLen);
}

std::string_view Uri::query() const
{
	if(!hasQuery())
		return {};

	return std::string_view(m_data).substr(m_schemeLen + m_authorityLen + m_pathLen,
	                                       m_queryLen);
}

std::string_view Uri::fragment() const
{
	if(!hasFragment())
		return {};

	return std::string_view(m_data).substr(m_schemeLen + m_authorityLen + m_pathLen + m_queryLen,
	                                       m_fragmentLen);
}

void Uri::insertScheme(std::string_view scheme)
{
	replaceStringRange(m_data, 0, m_schemeLen, scheme);
	m_schemeLen = static_cast<std::uint16_t>(scheme.size());
}

void Uri::insertAuthority(std::string_view authority)
{
	replaceStringRange(m_data, m_schemeLen, m_authorityLen, authority);
	m_authorityLen = static_cast<std::uint16_t>(authority.size());
	m_hasAuthority = 1;
}

void Uri::insertPath(std::string_view path)
{
	replaceStringRange(m_data,
	                   m_schemeLen + m_authorityLen,
	                   m_pathLen,
	                   path);
	m_pathLen = static_cast<std::uint16_t>(path.size());
}

void Uri::insertQuery(std::string_view query)
{
	replaceStringRange(m_data,
	                   m_schemeLen + m_authorityLen + m_pathLen,
	                   m_queryLen,
	                   query);
	m_queryLen = static_cast<std::uint16_t>(query.size());
	m_hasQuery = 1;
}

void Uri::insertFragment(std::string_view fragment)
{
	replaceStringRange(m_data,
	                   m_schemeLen + m_authorityLen + m_pathLen + m_queryLen,
	                   m_fragmentLen,
	                   fragment);
	m_fragmentLen = static_cast<std::uint16_t>(fragment.size());
	m_hasFragment = 1;
}

std::string Uri::encode(std::string_view decoded, std::string_view exclude)
{
	std::string encoded;
	encoded.reserve(decoded.size());

	for(const unsigned char c : decoded)
	{
		if(exclude.find(c) != std::string_view::npos ||
		   std::isalnum(c) ||
		   c == '_' ||
		   c == '.' ||
		   c == '-')
		{
			encoded.push_back(c);
		}
		else
		{
			constexpr auto hexLookup = "0123456789ABCDEF";
			encoded.push_back('%');
			encoded.push_back(hexLookup[c >> 4]);
			encoded.push_back(hexLookup[c & 0xF]);
		}
	}

	return encoded;
}

std::string Uri::decode(std::string_view encoded)
{
	std::string decoded;

	for(size_t i = 0; i < encoded.size(); ++i)
	{
		if(encoded[i] == '%' && i + 2 < encoded.size())
		{
			const char* start = &encoded[i + 1];
			const char* end   = &encoded[i + 3];

			unsigned char c;
			const auto [ptr, ec] = std::from_chars(start, end, c, 16);

			if(ec != std::errc{} || ptr != end)
				return {};

			decoded += c;
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
