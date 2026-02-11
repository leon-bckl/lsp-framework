#include <cassert>
#include <lsp/messagehandler.h>

namespace lsp{
namespace{

thread_local const MessageId* t_currentRequestId = nullptr;

json::Integer nextUniqueRequestId()
{
	static std::atomic<json::Integer> s_uniqueRequestId = 0;
	return ++s_uniqueRequestId;
}

}

MessageHandler::MessageHandler(Connection& connection, unsigned int maxResponseThreads)
	: m_connection{connection}
	, m_threadPool(0, maxResponseThreads)
{
}

void MessageHandler::processIncomingMessages()
{
	auto messageOrBatch = m_connection.readMessage();

	if(auto* const message = std::get_if<jsonrpc::Message>(&messageOrBatch))
	{
		if(auto* const request = std::get_if<jsonrpc::Request>(message))
		{
			auto optionalResponse = processRequest(std::move(*request), true);

			if(optionalResponse.has_value())
				m_connection.writeMessage(std::move(*optionalResponse));
		}
		else
		{
			processResponse(std::move(std::get<jsonrpc::Response>(*message)));
		}
	}
	else
	{
		auto& batch         = std::get<jsonrpc::MessageBatch>(messageOrBatch);
		auto  responseBatch = jsonrpc::MessageBatch();

		for(auto& message : batch)
		{
			if(auto* const request = std::get_if<jsonrpc::Request>(&message))
			{
				auto optionalResponse = processRequest(std::move(*request), false);

				if(optionalResponse.has_value())
					responseBatch.push_back(std::move(*optionalResponse));
			}
			else
			{
				processResponse(std::move(std::get<jsonrpc::Response>(message)));
			}
		}

		if(!responseBatch.empty())
			m_connection.writeMessage(std::move(responseBatch));
	}
}

const MessageId& MessageHandler::currentRequestId()
{
	assert(t_currentRequestId);
	if(!t_currentRequestId)
		throw std::logic_error("MessageHandler::currentRequestId called outside of a request context");

	return *t_currentRequestId;
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
		assert(!t_currentRequestId);
		if(request.id.has_value())
		{
			t_currentRequestId = &request.id.value();
		}
		else
		{
			static const MessageId NullMessageId = json::Null();
			t_currentRequestId = &NullMessageId;
		}

		try
		{
			lock.unlock();

			// Call handler for the method type and return optional response
			response = handlerIt->second(
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
					*request.id, MessageError::InvalidParams, e.what());
			}
		}
		catch(const std::exception& e)
		{
			if(!request.isNotification())
			{
				response = jsonrpc::createErrorResponse(
					*request.id, MessageError::InternalError, e.what());
			}
		}
		catch(...)
		{
			t_currentRequestId = nullptr;
			throw;
		}

		t_currentRequestId = nullptr;
	}
	else
	{
		if(!request.isNotification())
			response = jsonrpc::createErrorResponse(*request.id, MessageError::MethodNotFound, "Method not found");
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

	try
	{
		assert(!t_currentRequestId);
		t_currentRequestId = &response.id;

		if(response.result.has_value())
		{
			result->setValueFromJson(std::move(*response.result));
		}
		else // Error response received.
		{
			assert(response.error.has_value());
			auto& error = *response.error;
			result->setError(ResponseError(error.code, std::move(error.message), std::move(error.data)));
		}
	}
	catch(...)
	{
		t_currentRequestId = nullptr;
		throw;
	}

	t_currentRequestId = nullptr;
}

void MessageHandler::addHandler(std::string_view method, HandlerWrapper&& handlerFunc)
{
	std::lock_guard lock{m_requestHandlersMutex};
	m_requestHandlersByMethod[std::string(method)] = std::move(handlerFunc);
}

MessageHandler& MessageHandler::add(std::string_view method, GenericMessageCallback callback)
{
	addHandler(method,
		[f = std::move(callback)](json::Value&& params, bool) -> OptionalResponse
		{
			const auto isNotification = std::holds_alternative<std::nullptr_t>(currentRequestId());
			auto result = f(std::move(params));

			if(!isNotification)
				return jsonrpc::createResponse(currentRequestId(), std::move(result));

			return std::nullopt;
		}
	);

	return *this;
}

MessageHandler& MessageHandler::add(std::string_view method, GenericAsyncMessageCallback callback)
{
	addHandler(method,
		[this, f = std::move(callback)](json::Value&& params, bool allowAsync) -> OptionalResponse
		{
			const auto isNotification = std::holds_alternative<std::nullptr_t>(currentRequestId());
			auto future = f(std::move(params));

			if(allowAsync)
			{
				m_threadPool.addTask(
					[this, future = std::move(future), isNotification = isNotification, requestId = currentRequestId()]() mutable
					{
						auto response = createResponseFromAsyncResult<GenericMessage>(requestId, future);

						if(!isNotification)
							sendResponse(std::move(response));
					}
				);
				return std::nullopt;
			}

			auto result = future.get();

			if(!isNotification)
				return jsonrpc::createResponse(currentRequestId(), std::move(result));

			return std::nullopt;
		}
	);

	return *this;
}

void MessageHandler::sendResponse(jsonrpc::Response&& response)
{
	m_connection.writeMessage(std::move(response));
}

MessageId MessageHandler::sendRequest(std::string_view method, RequestResultPtr result, std::optional<json::Value>&& params)
{
	std::lock_guard lock{m_pendingRequestsMutex};
	const auto messageId = nextUniqueRequestId();
	m_pendingRequests[messageId] = std::move(result);
	auto request = jsonrpc::createRequest(messageId, method, std::move(params));
	m_connection.writeMessage(std::move(request));
	return messageId;
}

MessageId MessageHandler::sendRequest(
	std::string_view method,
	std::optional<json::Value>&& params,
	GenericResponseCallback then,
	GenericErrorResponseCallback error)
{
	auto result = std::make_unique<CallbackRequestResult<json::Value, decltype(then), decltype(error)>>(
		std::move(then), std::move(error));
	return sendRequest(method, std::move(result), std::move(params));
}

FutureResponse<MessageHandler::GenericMessage> MessageHandler::sendRequest(std::string_view method, std::optional<json::Value>&& params)
{
	auto result    = std::make_unique<FutureRequestResult<json::Value>>();
	auto future    = result->future();
	auto messageId = sendRequest(method, std::move(result), std::move(params));

	return {std::move(messageId), std::move(future)};
}

void MessageHandler::sendNotification(std::string_view method, std::optional<json::Value>&& params)
{
	auto notification = jsonrpc::createNotification(method, std::move(params));
	m_connection.writeMessage(std::move(notification));
}

} // namespace lsp
