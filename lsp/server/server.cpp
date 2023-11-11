#include "server.h"

#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/server/languageadapter.h>
#include <lsp/messagehandler.h>
#include <fstream>

namespace lsp::server{
namespace{

void sendErrorRespone(Connection& connection, types::ErrorCodes errorCode, const std::string& message, const jsonrpc::MessageId& messageId = json::Null{}){
	connection.sendMessage(jsonrpc::createErrorResponse(messageId, types::ErrorCodes{errorCode}.value(), message, std::nullopt));
}

} // namespace

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter){
	static_cast<void>(languageAdapter);
	Connection connection{in, out};
	int exitCode = EXIT_SUCCESS;

	for(;;){
		try{
		}catch(const json::ParseError& e){
			sendErrorRespone(connection, types::ErrorCodes::ParseError, std::string{"JSON parse error: "} + e.what());
		}catch(const jsonrpc::ProtocolError& e){
			sendErrorRespone(connection, types::ErrorCodes::InvalidRequest, std::string{"Message does not conform to the jsonrpc protocol: "} + e.what());
		}catch(const ProtocolError& e){
			sendErrorRespone(connection, types::ErrorCodes::InvalidRequest, std::string{"Base protocol error: "} + e.what());
		}catch(const ConnectionError&){
			exitCode = EXIT_FAILURE;
			break;
		}catch(...){
			sendErrorRespone(connection, types::ErrorCodes::UnknownErrorCode, "Unknown internal error");
			exitCode = EXIT_FAILURE;
			break;
		}
	}

	return exitCode;
}

} // namespace lsp::server
