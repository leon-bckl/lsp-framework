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
	jsonrpc::Response response;

	response.id = messageId;
	response.error = jsonrpc::ResponseError{types::ErrorCodes{errorCode}.value(), message, {}};

	connection.writeMessage(response);
}

} // namespace

int start(std::istream& in, std::ostream& out, LanguageAdapter& languageAdapter){
	static_cast<void>(languageAdapter);
	Connection connection{in, out};
	MessageHandler handler;
	int exitCode = EXIT_SUCCESS;

	handler.add<messages::Initialize>([](const auto& params, auto& result){
	 static_cast<void>(params);
	 result = {};
	}).add<messages::Shutdown>([](auto& result){
	 static_cast<void>(result);
	}).add<messages::Exit>([](){

	});

	for(;;){
		jsonrpc::MessagePtr message;

		try{
			message = connection.readNextMessage();

			if(message->isRequest()){
				auto* request = dynamic_cast<jsonrpc::Request*>(message.get());
				auto lspMessage = Message::createFromMethod(messages::methodFromString(request->method));

				if(lspMessage && handler.handlesMessage(lspMessage->method())){
					handler.processMessage(*lspMessage);

					if(lspMessage->method() == messages::Method::Exit)
						break;

					if(!request->isNotification() && lspMessage->isA<ClientToServerRequest>()){
						jsonrpc::Response response;
						response.id = request->id.value();
						response.result = lspMessage->as<ClientToServerRequest>().resultJson();
						connection.writeMessage(response);
					}
				}else if(!request->isNotification()){
					sendErrorRespone(connection, types::ErrorCodes::MethodNotFound, "Unsupported method type: " + request->method, request->id.value());
				}
			}
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
