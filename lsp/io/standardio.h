#pragma once

#include <istream>
#include <ostream>

namespace lsp::io{

std::istream& standardInput();
std::ostream& standardOutput();

} //namespace lsp::io
