#include "uri.h"

#include "str.h"

namespace lsp::util{

std::string FileURI::toString() const{
	std::string result{Scheme};
#ifdef _WIN32
	result += "/" + util::str::replace(m_path, ":", "%3A");
#else
	result += m_path;
#endif
	return result;
}

FileURI FileURI::fromString(std::string_view str){
	FileURI result;

	str = str::trimView(str);
	// Remove the scheme prefix if present
	auto idx = str.find(':');

	if(idx != std::string_view::npos
#ifdef _WIN32
		 && idx != 1 // Ignore the colon after the drive letter
#endif
	){
		if(str.starts_with(Scheme) && str.size() > Scheme.size()){
#ifdef _WIN32
			result.m_path = decode(str.substr(Scheme.size() + 1)); // Skip leading '/'
#else
			result.m_path = decode(str.substr(Scheme.size()));
#endif
		}
	}else{
		result.m_path = decode(str);
	}

	return result;
}

std::string FileURI::decode(std::string_view encoded){
	std::string decoded;

	for(size_t i = 0; i < encoded.size(); ++i){
		if(encoded[i] == '%'){
			if(i + 2 < encoded.size()){
				const char* start = &encoded[i + 1];
				const char* end = &encoded[i + 3];

				char c;
				auto [ptr, ec] = std::from_chars(start, end, c, 16);

				if(ec != std::errc{} || ptr != end)
					return {};

				decoded += c;
				i += 2;
			}else{
				return {};
			}
		}else{
			decoded += encoded[i];
		}
	}

	return decoded;
}

} // namespace lsp::util
