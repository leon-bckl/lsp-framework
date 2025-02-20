#include "fileuri.h"

#include <cctype>
#include <charconv>
#ifdef _WIN32
#include <filesystem>
#endif
#include "str.h"

namespace lsp{

std::string FileURI::toString() const
{
#ifdef _WIN32
	std::filesystem::path path{m_path};
	if(path.is_absolute())
	{
		std::u8string u8Path = path.u8string();
		return Scheme + '/' + encode(std::string_view{reinterpret_cast<const char*>(u8Path.data()), u8Path.size()});
	}
#endif

	return Scheme + encode(m_path);
}

std::string FileURI::fromString(std::string_view str)
{
	str = str::trimView(str);

	if(str.starts_with(Scheme))
		str.remove_prefix(Scheme.size());
#ifdef _WIN32
	if(!str.empty() && str[0] == '/')
		str.remove_prefix(1);
#endif
	return decode(str);
}

std::string FileURI::encode(std::string_view decoded)
{
	std::string encoded;
	encoded.reserve(decoded.size());

	for(const unsigned char c : decoded)
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
