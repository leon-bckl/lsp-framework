#include <cassert>
#define LSP_ERROR_DONT_INCLUDE_GENERATED_TYPES
#include <lsp/error.h>
#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{
namespace{

json::Integer nextUniqueRequestId()
{
	static std::atomic<json::Integer> g_uniqueRequestId = 0;
	return ++g_uniqueRequestId;
}

}

MessageHandler::MessageHandler(Connection& connection)
	: m_connection{connection}
{
	m_asyncResponseThread = std::thread{[this](){
		while(true)
		{
			std::unique_lock lock{m_pendingResponsesMutex};
			std::erase_if(m_pendingResponses,
				[this](const auto& p)
				{
					const auto& result = p.second;
					const auto  ready  = result->isReady();

					if(ready)
						m_connection.sendResponse(result->createResponse());

					return ready;
				});

			if(!m_running)
				break;

			lock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds{100}); // TODO: Do this in a better way
		}
	}};
}

MessageHandler::~MessageHandler()
{
	{
		std::lock_guard lock{m_pendingResponsesMutex};
		m_running = false;
	}

	m_asyncResponseThread.join();
}

void MessageHandler::processIncomingMessages()
{
	m_connection.receiveNextMessage(*this, *this);
}

void MessageHandler::remove(MessageMethod method)
{
	const auto idx = static_cast<std::size_t>(method);

	if(idx < m_requestHandlers.size())
		m_requestHandlers[idx] = nullptr;
}

void MessageHandler::onRequest(jsonrpc::Request&& request)
{
	auto response = processRequest(std::move(request), true);

	if(response.has_value())
		m_connection.sendResponse(std::move(*response));
}

void MessageHandler::onRequestBatch(jsonrpc::RequestBatch&& batch)
{
	jsonrpc::ResponseBatch responses;
	responses.reserve(batch.size());

	for(auto&& r : batch)
	{
		auto response = processRequest(std::move(r), false);

		if(response.has_value())
			responses.push_back(std::move(*response));
	}

	if(!responses.empty())
		m_connection.sendResponseBatch(std::move(responses));
}

MessageHandler::OptionalResponse MessageHandler::processRequest(jsonrpc::Request&& request, bool allowAsync)
{
	const auto method = messageMethodFromString(request.method);
	const auto index  = static_cast<std::size_t>(method);
	OptionalResponse response;

	if(index < m_requestHandlers.size() && m_requestHandlers[index])
	{
		try
		{
			// Call handler for the method type and return optional response
			response = m_requestHandlers[index](
				request.id.has_value() ? *request.id : json::Null{},
				request.params.has_value() ? std::move(*request.params) : json::Null{},
				allowAsync);
		}
		catch(const RequestError& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
					*request.id, e.code(), e.what(), e.data());
			}
		}
		catch(const json::TypeError& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
					*request.id, jsonrpc::Error::InvalidParams, e.what());
			}
		}
		catch(const std::exception& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
					*request.id, jsonrpc::Error::InternalError, e.what());
			}
		}
	}
	else if(!request.isNotification())
	{
		response = jsonrpc::createErrorResponse(
			*request.id, jsonrpc::Error::MethodNotFound, "Unsupported method: " + request.method);
	}

	return response;
}

void MessageHandler::addHandler(MessageMethod method, HandlerWrapper&& handlerFunc)
{
	std::lock_guard lock{m_requestHandlersMutex};
	const auto index = static_cast<std::size_t>(method);

	if(m_requestHandlers.size() <= index)
		m_requestHandlers.resize(index + 1);

	m_requestHandlers[index] = std::move(handlerFunc);
}

void MessageHandler::addResponseResult(const MessageId& id, ResponseResultPtr result)
{
	std::lock_guard lock{m_pendingResponsesMutex};
	const auto it = m_pendingResponses.find(id);

	if(it != m_pendingResponses.end())
		throw RequestError{jsonrpc::Error::InvalidRequest, "Request id is not unique"};

	m_pendingResponses.emplace(id, std::move(result));
}

void MessageHandler::onResponse(jsonrpc::Response&& response)
{
	RequestResultPtr result;

	// Find pending request for the response that was received based on the message id.
	{
		std::lock_guard lock{m_pendingRequestsMutex};
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
			result->setValueFromJson(std::move(*response.result));
		}
		catch(const json::TypeError& e)
		{
			result->setException(std::make_exception_ptr(e));
		}
	}
	else // Error response received. Create an exception.
	{
		assert(response.error.has_value());
		const auto& error = *response.error;
		result->setException(std::make_exception_ptr(ResponseError{error.code, error.message, error.data}));
	}
}

void MessageHandler::onResponseBatch(jsonrpc::ResponseBatch&& batch)
{
	// This should never be called as no batches are ever sent
	for(auto&& r : batch)
		onResponse(std::move(r));
}

MessageId MessageHandler::sendRequest(MessageMethod method, RequestResultPtr result, const std::optional<json::Any>& params)
{
	std::lock_guard lock{m_pendingRequestsMutex};
	auto messageId = nextUniqueRequestId();
	m_pendingRequests[messageId] = std::move(result);
	auto methodStr = messageMethodToString(method);
	m_connection.sendRequest(jsonrpc::createRequest(messageId, methodStr, params));
	return messageId;
}

void MessageHandler::sendNotification(MessageMethod method, const std::optional<json::Any>& params)
{
	const auto methodStr = messageMethodToString(method);
	auto notification = jsonrpc::createNotification(methodStr, params);
	m_connection.sendRequest(std::move(notification));
}

} // namespace lsp
