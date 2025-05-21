#pragma once

#include <string>
#include <cstddef>

namespace lsp::io{

class Stream{
public:
	Stream& operator=(const Stream&) = delete;

	static constexpr char Eof = std::char_traits<char>::eof();

	virtual void read(char* buffer, std::size_t size) = 0;
	virtual void write(const char* buffer, std::size_t size) = 0;
};

} // namespace lsp::io
