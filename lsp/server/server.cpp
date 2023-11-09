#include "server.h"

#include <lsp/types.h>
#include <lsp/server/languageadapter.h>

#include "connection.h"

namespace lsp::server{
/*
 * Server
 */

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter){
	Connection connection{in, out};

	for(;;){
		try{
			jsonrpc::MessagePtr message = connection.readNextMessage();
			static_cast<void>(message);
			static_cast<void>(languageAdapter);
		}catch(const json::ParseError&){
			return EXIT_FAILURE;
		}catch(const ProtocolError&){
			return EXIT_FAILURE;
		}catch(const ConnectionError&){
			return EXIT_FAILURE;
		}catch(const jsonrpc::ProtocolError&){
			return EXIT_FAILURE;
		}catch(...){
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

}
