#pragma once

#include <string>
#include <charconv>
#include <functional>
#include <string_view>
#include "str.h"

namespace lsp::util{

/*
 * Really simple uri class that only supports the file:// scheme
 */
class FileURI{
public:
	inline static constexpr std::string_view Scheme{"file://"};

	FileURI() = default;
	FileURI(std::string_view in){ *this = fromString(in); }
	FileURI(const std::string& in){ *this = fromString(in); }
	FileURI& operator=(std::string_view other){ *this = fromString(other); return *this; }
	FileURI& operator=(const std::string& other){ *this = fromString(other); return *this; }
	bool operator==(const FileURI& other) const{ return path() == other.path(); }
	bool operator!=(const FileURI& other) const{ return path() != other.path(); }
	bool operator==(std::string_view other) const{ return path() == other; }
	bool operator!=(std::string_view other) const{ return path() != other; }
	bool operator<(const FileURI& other) const{ return path() < other.path(); }

	const std::string& path() const{ return m_path; }
	std::string toString() const{ return std::string{Scheme} + path(); }
	bool isValid() const{ return !m_path.empty(); }
	void clear(){ m_path.clear(); }

	static FileURI fromString(std::string_view str);

private:
	std::string m_path;

	static std::string decode(std::string_view encoded);
};

} // namespace lsp::util

namespace std{

template<>
struct hash<lsp::util::FileURI>{
	size_t operator()(const lsp::util::FileURI& uri) const{
		return hash<string>{}(uri.path());
	}
};

} // namespace std
