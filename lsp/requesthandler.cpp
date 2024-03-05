#include "messagehandler.h"

#include <lsp/types.h>
#include <lsp/connection.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include "error.h"

namespace lsp{

RequestHandler::RequestHandler(Connection& connection) : m_connection{connection}
{
	m_responseThread = std::thread{[this](){
		while(true)
		{
			std::unique_lock lock{m_pendingResponsesMutex};
			m_pendingResponses.erase(std::find_if(m_pendingResponses.begin(), m_pendingResponses.end(),
				[this](const auto& p)
				{
					const auto& result = p.second;
					const auto ready = result->isReady();

					if(ready)
					{
						try
						{
							m_connection.sendResponse(result->get());
						}
						catch(const RequestError& e)
						{
							sendErrorMessage(ErrorCodes{e.code()}, e.what());
						}
					}

					return ready;
				}),
				m_pendingResponses.end());

			if(!m_running)
				break;

			lock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds{100}); // TODO: Implement non-busy waiting solution
		}
	}};
}

RequestHandler::~RequestHandler()
{
	{
		std::lock_guard lock{m_pendingResponsesMutex};
		m_running = false;
	}

	m_responseThread.join();
}

void RequestHandler::onRequest(jsonrpc::Request&& request)
{
	auto response = processRequest(std::move(request), true);

	if(response.has_value())
		m_connection.sendResponse(std::move(*response));
}

void RequestHandler::onRequestBatch(jsonrpc::RequestBatch&& batch)
{
	jsonrpc::ResponseBatch responses;
	responses.reserve(batch.size());

	for(auto&& r : batch)
	{
		auto response = processRequest(std::move(r), false);

		if(response.has_value())
			responses.push_back(std::move(*response));
	}

	m_connection.sendResponseBatch(std::move(responses));
}

RequestHandler::OptionalResponse RequestHandler::processRequest(jsonrpc::Request&& request, bool allowASync)
{
	const auto method = messageMethodFromString(request.method);
	const auto index  = static_cast<std::size_t>(method);
	OptionalResponse response;

	if(index <= m_requestHandlers.size() && m_requestHandlers[index])
	{
		try
		{
			// Call handler for the method type and return optional response
			response = m_requestHandlers[static_cast<std::size_t>(method)](
				request.id.has_value() ? *request.id : json::Null{},
				request.params.has_value() ? std::move(*request.params) : json::Null{},
				allowASync
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

void RequestHandler::addHandler(MessageMethod method, HandlerWrapper&& handlerFunc)
{
	std::lock_guard lock{m_requestHandlersMutex};
	const auto index = static_cast<std::size_t>(method);

	if(m_requestHandlers.size() <= index)
		m_requestHandlers.resize(index + 1);

	m_requestHandlers[index] = std::move(handlerFunc);
}

void RequestHandler::addResponseResult(const jsonrpc::MessageId& id, ResponseResultPtr result)
{
	std::lock_guard lock{m_pendingResponsesMutex};
	auto it = m_pendingResponses.find(id);

	if(it != m_pendingResponses.end())
		throw RequestError{ErrorCodes::InvalidRequest, "Request id is not unique"};

	m_pendingResponses.emplace(id, std::move(result));
}

void RequestHandler::sendErrorMessage(ErrorCodes code, const std::string& message)
{
	m_connection.sendResponse(jsonrpc::createErrorResponse(json::Null{}, code, message, std::nullopt));
}

} // namespace lsp
