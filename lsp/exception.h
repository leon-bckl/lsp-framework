#pragma once

#include <stdexcept>

namespace lsp{

class Exception : public std::runtime_error{
protected:
	using std::runtime_error::runtime_error;
};

} // namespace lsp
