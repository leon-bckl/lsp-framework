#include "messagehandler.h"

#include <lsp/connection.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/types.h>
#include "error.h"

namespace lsp{

std::optional<jsonrpc::Response> MessageHandler::processRequest(const jsonrpc::Request& request)
{
	auto method = messageMethodFromString(request.method);
	std::optional<jsonrpc::Response> response;

	if(handlesRequest(method))
	{
		try
		{
			// Call handler for the method type and return optional response
			response = m_requestHandlers[static_cast<std::size_t>(method)](
				request.id.has_value() ? *request.id : json::Null{},
				request.params.has_value() ? *request.params : json::Null{}
			);
		}
		catch(const json::TypeError& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
				 *request.id, ErrorCodes{ErrorCodes::InvalidParams}, e.what());
			}
		}
		catch(const RequestError& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
				 *request.id, e.code(), e.what(), e.data());
			}
		}
	}
	else if(!request.isNotification())
	{
		response = jsonrpc::createErrorResponse(
		 *request.id, ErrorCodes::MethodNotFound, "Unsupported method: " + request.method);
	}

	return response;
}

void MessageHandler::processResponse(const jsonrpc::Response& response)
{
	ResponseResultPtr result;

	{
		std::lock_guard lock{m_requestMutex};
		if(auto it = m_pendingRequests.find(response.id); it != m_pendingRequests.end())
		{
			result = std::move(it->second);
			m_pendingRequests.erase(it);
		}
	}

	if(!result) // If there's no result it means a response was received without a request which makes no sense but just ignore it...
		return;

	if(response.result.has_value())
	{
		try
		{
			result->setValueFromJson(*response.result);
		}
		catch(const json::TypeError& e)
		{
			result->setException(std::make_exception_ptr(e));
		}
	}
	else
	{
		assert(response.error.has_value());
		const auto& error = *response.error;
		result->setException(std::make_exception_ptr(ResponseError{error.message, ErrorCodes{error.code}, error.data}));
	}
}

std::optional<jsonrpc::Response> MessageHandler::processMessage(const jsonrpc::Message& message)
{
	if(message.isRequest())
		return processRequest(static_cast<const jsonrpc::Request&>(message));

	processResponse(static_cast<const jsonrpc::Response&>(message));
	return std::nullopt;
}

jsonrpc::MessageBatch MessageHandler::processMessageBatch(const jsonrpc::MessageBatch& batch)
{
	jsonrpc::MessageBatch responseBatch;
	responseBatch.reserve(batch.size());

	for(const auto& m : batch)
	{
		auto response = processMessage(*m);

		if(response.has_value())
			responseBatch.push_back(std::make_unique<jsonrpc::Response>(std::move(*response)));
	}

	return responseBatch;
}

void MessageHandler::processIncomingMessages()
{
	try
	{
		auto incoming = m_connection.receiveMessage();

		if(std::holds_alternative<jsonrpc::MessagePtr>(incoming))
		{
			auto response = processMessage(*std::get<jsonrpc::MessagePtr>(incoming));

			if(response.has_value())
				m_connection.sendMessage(*response);
		}
		else
		{
			assert(std::holds_alternative<jsonrpc::MessageBatch>(incoming));
			auto responseBatch = processMessageBatch(std::get<jsonrpc::MessageBatch>(incoming));

			if(!responseBatch.empty())
				m_connection.sendMessageBatch(responseBatch);
		}
	}
	catch(const json::ParseError& e)
	{
		sendErrorMessage(ErrorCodes::ParseError, std::string{"JSON parse error: "} + e.what());
	}
	catch(const jsonrpc::ProtocolError& e)
	{
		sendErrorMessage(ErrorCodes::InvalidRequest, std::string{"Message does not conform to the jsonrpc protocol: "} + e.what());
	}
	catch(const ProtocolError& e)
	{
		sendErrorMessage(ErrorCodes::InvalidRequest, std::string{"Base protocol error: "} + e.what());
	}
	catch(const ConnectionError&)
	{
		throw;
	}
	catch(...)
	{
		sendErrorMessage(ErrorCodes::UnknownErrorCode, "Unknown internal error");
		throw;
	}
}

bool MessageHandler::handlesRequest(MessageMethod method)
{
	auto index = static_cast<std::size_t>(method);
	return index < m_requestHandlers.size() && m_requestHandlers[index];
}

void MessageHandler::addHandler(MessageMethod method, HandlerWrapper&& handlerFunc)
{
	const auto index = static_cast<std::size_t>(method);

	if(m_requestHandlers.size() <= index)
		m_requestHandlers.resize(index + 1);

	m_requestHandlers[index] = std::move(handlerFunc);
}

void MessageHandler::sendRequest(MessageMethod method, ResponseResultPtr result, const std::optional<json::Any>& params)
{
	std::lock_guard lock{m_requestMutex};
	auto id = m_uniqueRequestId++;
	assert(!m_pendingRequests.contains(id));
	m_pendingRequests[id] = std::move(result);
	auto methodStr = messageMethodToString(method);
	m_connection.sendMessage(jsonrpc::createRequest(id, methodStr, params));
}

void MessageHandler::sendNotification(MessageMethod method, const std::optional<json::Any>& params)
{
	auto methodStr = std::string{messageMethodToString(method)};
	m_connection.sendMessage(jsonrpc::createNotification(methodStr, params));
}

void MessageHandler::sendErrorMessage(ErrorCodes code, const std::string& message)
{
	m_connection.sendMessage(jsonrpc::createErrorResponse(json::Null{}, code, message, std::nullopt));
}

} // namespace lsp
