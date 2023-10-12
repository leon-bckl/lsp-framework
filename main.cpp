#include <lsp/server.h>
#include <json/json.h>
#include <uscript/uscript.h>
#include <jsonrpc/jsonrpc.h>
#include <iostream>

int main(){
	uscript::initialize();

	lsp::Server server;

	return server.run(std::cin, std::cout);
}
