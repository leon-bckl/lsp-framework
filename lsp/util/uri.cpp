#include "uri.h"

#include <cctype>
#include <charconv>
#include <filesystem>
#include "str.h"

namespace lsp::util{

std::string FileURI::toString() const
{
	return std::string{Scheme} +
#ifdef _WIN32
		"/" +
#endif
		encode(m_path);
}

std::string FileURI::fromString(std::string_view str)
{
	str = str::trimView(str);

	if(str.starts_with(Scheme))
		str.remove_prefix(Scheme.size());

	return std::filesystem::canonical(
#ifdef _WIN32
			decode(str.substr(Scheme.size() + (!str.empty() && str[0] == '/'))) // Skip leading '/' for absolute paths
#else
			decode(str.substr(Scheme.size()))
#endif
		).string();
}

std::string FileURI::encode(std::string_view decoded)
{
	std::string encoded;
	encoded.reserve(decoded.size());

	for(char c : decoded)
	{
		if(std::isalnum(c) || c == '/' || c == '_')
		{
			encoded.push_back(c);
		}
		else
		{
			const char* hexLookup = "0123456789ABCDEF";
			encoded.push_back('%');
			encoded.push_back(hexLookup[c >> 4]);
			encoded.push_back(hexLookup[c & 0xF]);
		}
	}

	return encoded;
}

std::string FileURI::decode(std::string_view encoded)
{
	std::string decoded;

	for(size_t i = 0; i < encoded.size(); ++i)
	{
		if(encoded[i] == '%' && i + 2 < encoded.size())
		{
			const char* start = &encoded[i + 1];
			const char* end = &encoded[i + 3];

			char c;
			auto [ptr, ec] = std::from_chars(start, end, c, 16);

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

} // namespace lsp::util
