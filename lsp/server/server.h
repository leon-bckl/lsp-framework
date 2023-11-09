#pragma once

#include <stdexcept>

namespace lsp::server{

class LanguageAdapter;

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter);

}
