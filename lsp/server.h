#pragma once

#include <istream>
#include <ostream>

namespace lsp{

class Server{
public:
	int run(std::istream& in, std::ostream& out);
};

}
