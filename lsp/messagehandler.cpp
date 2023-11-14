#include "messagehandler.h"

#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include "error.h"

namespace lsp{

jsonrpc::ResponsePtr MessageHandler::processRequest(const jsonrpc::Request& request){
	auto method = messages::methodFromString(request.method);

	if(!handlesRequest(method)){
		if(!request.isNotification())
			return jsonrpc::createErrorResponse(request.id.value(), types::ErrorCodes::MethodNotFound, "Unsupported method: " + request.method, std::nullopt);

		return nullptr;
	}

	jsonrpc::MessageId id = json::Null{};

	if(request.id.has_value()) // Notifications don't have an id
		id = request.id.value();

	return m_requestHandlers[static_cast<std::size_t>(method)](id, request.params.has_value() ? request.params.value() : json::Null{});
}

void MessageHandler::processResponse(const jsonrpc::Response& response){
	std::unique_lock lock{m_requestMutex};

	if(auto it = m_pendingRequests.find(response.id); it == m_pendingRequests.end()){
		ResponseResultPtr result = std::move(it->second);
		m_pendingRequests.erase(it);
		lock.unlock();

		if(response.result.has_value()){
			try{
				result->setValueFromJson(response.result.value());
			}catch(const json::TypeError& e){
				result->setException(std::make_exception_ptr(e));
			}
		}else if(response.error.has_value()){
			const auto& error = response.error.value();
			result->setException(std::make_exception_ptr(ResponseError{error.message, types::ErrorCodes{error.code}, error.data}));
		}
	}
}

jsonrpc::ResponsePtr MessageHandler::processMessage(const jsonrpc::Message& message){
	try{
		if(message.isRequest())
			return processRequest(static_cast<const jsonrpc::Request&>(message));

		processResponse(static_cast<const jsonrpc::Response&>(message));
	}catch(const json::TypeError& e){
		if(message.isRequest()){
			const auto& request = static_cast<const jsonrpc::Request&>(message);

			if(!request.isNotification())
				return jsonrpc::createErrorResponse(request.id.value(), types::ErrorCodes{types::ErrorCodes::InvalidParams}, e.what());
		}
	}catch(const RequestError& e){
		if(message.isRequest()){
			const auto& request = static_cast<const jsonrpc::Request&>(message);

			if(!request.isNotification())
				return jsonrpc::createErrorResponse(request.id.value(), e.code(), e.what(), e.data());
		}
	}

	return nullptr;
}

jsonrpc::MessageBatch MessageHandler::processBatch(const jsonrpc::MessageBatch& batch){
	jsonrpc::MessageBatch responseBatch;
	responseBatch.reserve(batch.size());

	for(const auto& m : batch){
		jsonrpc::MessagePtr response = processMessage(*m);

		if(m->isRequest() && !static_cast<const jsonrpc::Request*>(m.get())->isNotification()){
			assert(response);
			responseBatch.push_back(std::move(response));
		}
	}

	return responseBatch;
}

void MessageHandler::processIncomingMessages(){
	try{
		auto result = m_connection.receiveMessage();

		if(std::holds_alternative<jsonrpc::MessagePtr>(result)){
			const auto* message = std::get<jsonrpc::MessagePtr>(result).get();
			auto response = processMessage(*message);

			if(message->isRequest() && !static_cast<const jsonrpc::Request*>(message)->isNotification()){
				assert(response);
				m_connection.sendMessage(*response);
			}
		}else{
			auto responseBatch = processBatch(std::get<jsonrpc::MessageBatch>(result));

			if(!responseBatch.empty())
				m_connection.sendMessageBatch(responseBatch);
		}
	}catch(const json::ParseError& e){
		sendErrorMessage(types::ErrorCodes::ParseError, std::string{"JSON parse error: "} + e.what());
	}catch(const jsonrpc::ProtocolError& e){
		sendErrorMessage(types::ErrorCodes::InvalidRequest, std::string{"Message does not conform to the jsonrpc protocol: "} + e.what());
	}catch(ProtocolError& e){
		sendErrorMessage(types::ErrorCodes::InvalidRequest, std::string{"Base protocol error: "} + e.what());
	}catch(...){
		sendErrorMessage(types::ErrorCodes::UnknownErrorCode, "Unknown internal error");
		throw;
	}
}

bool MessageHandler::handlesRequest(messages::Method method){
	auto index = static_cast<std::size_t>(method);
	return index < m_requestHandlers.size() && m_requestHandlers[index];
}

void MessageHandler::addHandler(messages::Method method, HandlerWrapper&& handlerFunc){
	const auto index = static_cast<std::size_t>(method);

	if(m_requestHandlers.size() <= index)
		m_requestHandlers.resize(index + 1);

	m_requestHandlers[index] = std::move(handlerFunc);
}

void MessageHandler::sendRequest(messages::Method method, const jsonrpc::MessageId& id, ResponseResultPtr result, const std::optional<json::Any>& params){
	assert(method > messages::Method::INVALID && method < messages::Method::MAX_VALUE);

	std::lock_guard lock{m_requestMutex};
	assert(!m_pendingRequests.contains(id));

	if(m_pendingRequests.contains(id))
		throw std::logic_error{"Duplicate request id"};

	m_pendingRequests[id] = std::move(result);

	auto methodStr = std::string{messages::methodToString(method)};
	m_connection.sendMessage(*jsonrpc::createRequest(id, methodStr, params));
}

void MessageHandler::sendNotification(messages::Method method, const std::optional<json::Any>& params){
	auto methodStr = std::string{messages::methodToString(method)};
	m_connection.sendMessage(*jsonrpc::createNotification(methodStr, params));
}

void MessageHandler::sendErrorMessage(types::ErrorCodes code, const std::string& message){
	m_connection.sendMessage(*jsonrpc::createErrorResponse(json::Null{}, code, message, std::nullopt));
}

} // namespace lsp
