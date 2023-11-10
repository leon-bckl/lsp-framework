#include "server.h"

#include <lsp/types.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/server/languageadapter.h>
#include <lsp/messages.h>

#include "connection.h"

namespace lsp::server{
namespace{

void sendErrorRespone(Connection& connection, ErrorCodes errorCode, const std::string& message, const jsonrpc::MessageId& messageId = json::Null{}){
	jsonrpc::Response response;

	response.id = messageId;
	response.error = jsonrpc::ResponseError{ErrorCodes{errorCode}.value(), message, {}};

	connection.writeMessage(response);
}

} // namespace

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter){
	Connection connection{in, out};

	for(;;){
		jsonrpc::MessagePtr  message;

		try{
			message = connection.readNextMessage();
			static_cast<void>(message);
			static_cast<void>(languageAdapter);
		}catch(const json::ParseError& e){
			sendErrorRespone(connection, ErrorCodes::ParseError, std::string{"JSON parse error: "} + e.what());
		}catch(const jsonrpc::ProtocolError& e){
			sendErrorRespone(connection, ErrorCodes::InvalidRequest, std::string{"Message does not conform to the jsonrpc protocol: "} + e.what());
		}catch(const ProtocolError& e){
			sendErrorRespone(connection, ErrorCodes::ParseError, std::string{"Base protocol error: "} + e.what());
		}catch(const ConnectionError&){
			return EXIT_FAILURE;
		}catch(...){
			sendErrorRespone(connection, ErrorCodes::UnknownErrorCode, "Unknown internal error");
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

} // namespace lsp::server
