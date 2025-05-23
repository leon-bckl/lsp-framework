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
						m_connection.writeMessage(jsonrpc::responseToJson(result->createResponse()));

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
	auto messageJson = m_connection.readMessage();

	if(messageJson.isObject())
	{
		auto message = jsonrpc::messageFromJson(std::move(messageJson.object()));

		if(auto* request = std::get_if<jsonrpc::Request>(&message); request)
		{
			auto response = processRequest(std::move(*request), true);

			if(response.has_value())
				m_connection.writeMessage(jsonrpc::responseToJson(std::move(*response)));
		}
		else
		{
			auto response = std::get<jsonrpc::Response>(message);
			processResponse(std::move(response));
		}
	}
	else if(messageJson.isArray())
	{
		auto messageBatch = jsonrpc::messageBatchFromJson(std::move(messageJson.array()));

		if(auto* requests = std::get_if<jsonrpc::RequestBatch>(&messageBatch))
		{
			jsonrpc::ResponseBatch responses;
			responses.reserve(requests->size());

			for(auto&& r : *requests)
			{
				auto response = processRequest(std::move(r), false);

				if(response.has_value())
					responses.push_back(std::move(*response));
			}

			if(!responses.empty())
				m_connection.writeMessage(jsonrpc::responseBatchToJson(std::move(responses)));
		}
		else
		{
			auto& responses = std::get<jsonrpc::ResponseBatch>(messageBatch);
			// This should never be called as no batches are ever sent
			for(auto&& r : responses)
				processResponse(std::move(r));
		}
	}
	else
	{
		throw jsonrpc::ProtocolError{"Expected message to be a json object or array"};
	}
}

void MessageHandler::remove(std::string_view method)
{
	std::lock_guard lock{m_requestHandlersMutex};

	if(const auto it = m_requestHandlersByMethod.find(method); it != m_requestHandlersByMethod.end())
		m_requestHandlersByMethod.erase(it);
}

MessageHandler::OptionalResponse MessageHandler::processRequest(jsonrpc::Request&& request, bool allowAsync)
{
	std::unique_lock lock{m_requestHandlersMutex};
	OptionalResponse response;

	if(const auto handlerIt = m_requestHandlersByMethod.find(request.method);
	   handlerIt != m_requestHandlersByMethod.end() && handlerIt->second)
	{
		try
		{
			lock.unlock();

			// Call handler for the method type and return optional response
			response = handlerIt->second(
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
	else
	{
		if(!request.isNotification())
		{
			response = jsonrpc::createErrorResponse(
			 *request.id, jsonrpc::Error::MethodNotFound, "Unsupported method: " + request.method);
		}
	}

	return response;
}

void MessageHandler::processResponse(jsonrpc::Response&& response)
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

void MessageHandler::addHandler(std::string_view method, HandlerWrapper&& handlerFunc)
{
	std::lock_guard lock{m_requestHandlersMutex};
	m_requestHandlersByMethod[std::string(method)] = std::move(handlerFunc);
}

void MessageHandler::addResponseResult(const MessageId& id, ResponseResultPtr result)
{
	std::lock_guard lock{m_pendingResponsesMutex};
	const auto it = m_pendingResponses.find(id);

	if(it != m_pendingResponses.end())
		throw RequestError{jsonrpc::Error::InvalidRequest, "Request id is not unique"};

	m_pendingResponses.emplace(id, std::move(result));
}

MessageId MessageHandler::sendRequest(std::string_view method, RequestResultPtr result, const std::optional<json::Any>& params)
{
	std::lock_guard lock{m_pendingRequestsMutex};
	const auto messageId = nextUniqueRequestId();
	m_pendingRequests[messageId] = std::move(result);
	auto request = jsonrpc::createRequest(messageId, method, params);
	m_connection.writeMessage(jsonrpc::requestToJson(std::move(request)));
	return messageId;
}

void MessageHandler::sendNotification(std::string_view method, const std::optional<json::Any>& params)
{
	auto notification = jsonrpc::createNotification(method, params);
	m_connection.writeMessage(jsonrpc::requestToJson(std::move(notification)));
}

} // namespace lsp
