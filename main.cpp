#include <lsp/server.h>
#include <uscript/uscript.h>
#include <iostream>

int main(){
	uscript::initialize();

	lsp::Server server{std::cin, std::cout};

	return server.run();
}
