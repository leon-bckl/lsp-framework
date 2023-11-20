#include "standardio.h"

#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <cstdio>
#include <fcntl.h>
#endif

namespace lsp::io{

std::istream& standardInput()
{
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
#endif
	return std::cin;
}

std::ostream& standardOutput()
{
#ifdef _WIN32
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	return std::cout;
}

} // namespace lsp::io
