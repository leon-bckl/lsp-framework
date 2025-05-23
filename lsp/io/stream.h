#pragma once

#include <string>
#include <cstddef>
#include <stdexcept>

namespace lsp::io{

/*
 * Error
 */

class Error : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

/*
 * Stream
 */

class Stream{
public:
	Stream& operator=(const Stream&) = delete;

	static constexpr char Eof = std::char_traits<char>::eof();

	virtual void read(char* buffer, std::size_t size) = 0;
	virtual void write(const char* buffer, std::size_t size) = 0;
};

} // namespace lsp::io
