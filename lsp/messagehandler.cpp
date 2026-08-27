#include <cassert>
#include <lsp/messagehandler.h>

namespace lsp{
namespace{

thread_local const MessageId* t_currentRequestId = nullptr;

} // namespace

MessageHandler::MessageHandler(Connection connection, unsigned int maxResponseThreads)
	: m_connection{std::move(connection)}
	, m_threadPool(0, maxResponseThreads)
{
}

void MessageHandler::processNextMessage()
{
	auto messageOrBatch = m_connection.readMessage();

	if(auto* const message = std::get_if<jsonrpc::Message>(&messageOrBatch))
	{
		if(auto* const request = std::get_if<jsonrpc::Request>(message))
			processRequest(std::move(*request), nullptr);
		else
			processResponse(std::move(std::get<jsonrpc::Response>(*message)));
	}
	else
	{
		auto& batch       = std::get<jsonrpc::MessageBatch>(messageOrBatch);
		auto  batchSender = m_connection.messageBatch();

		for(auto& msg : batch)
		{
			if(auto* const request = std::get_if<jsonrpc::Request>(&msg))
				processRequest(std::move(*request), &batchSender);
			else
				processResponse(std::move(std::get<jsonrpc::Response>(msg)));
		}

		if(batchSender.batchIsEmpty())
			batchSender.discard();
		else
			batchSender.submit();
	}
}

void MessageHandler::setConnection(Connection connection)
{
	m_connection = std::move(connection);
}

const MessageId& MessageHandler::currentRequestId()
{
	if(!t_currentRequestId)
		throw std::logic_error("MessageHandler::currentRequestId called outside of a request context");

	return *t_currentRequestId;
}

void MessageHandler::remove(const std::string& method)
{
	std::lock_guard lock{m_requestHandlersMutex};

	if(const auto it = m_requestHandlersByMethod.find(method); it != m_requestHandlersByMethod.end())
		m_requestHandlersByMethod.erase(it);
}

void MessageHandler::processRequest(jsonrpc::Request&& request, Connection::BatchSender* batchSender)
{
	auto lock = std::unique_lock(m_requestHandlersMutex);

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
			handlerIt->second(
				request.params.has_value() ? std::move(*request.params) : json::Null{},
				batchSender);
		}
		catch(const RequestError& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, e.code(), e.what(), e.data());
		}
		catch(const json::TypeError& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, MessageError::InvalidParams, e.what());
		}
		catch(const std::exception& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, MessageError::InternalError, e.what());
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
			sendErrorResponse(*request.id, MessageError::MethodNotFound, "Method not found");
	}
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
		[this, f = std::move(callback)](json::Value&& params, Connection::BatchSender* batchSender)
		{
			const auto& requestId      = currentRequestId();
			const auto  isNotification = std::holds_alternative<std::nullptr_t>(requestId);
			const auto  result         = f(std::move(params));

			if(batchSender)
			{
				if(!isNotification)
				{
					auto responseWriter = batchSender->writeResponse(requestId);
					responseWriter.writeData(
						[](std::string_view key, const auto& value, json::ObjectWriter& writer)
						{
							writeJson(key, value, writer);
						}, result);
				}
			}
			else if(!isNotification)
			{
				sendResponse(requestId, result);
			}
		}
	);

	return *this;
}

MessageHandler& MessageHandler::add(std::string_view method, GenericAsyncMessageCallback callback)
{
	addHandler(method,
		[this, f = std::move(callback)](json::Value&& params, Connection::BatchSender* batchSender)
		{
			const auto& requestId      = currentRequestId();
			const auto  isNotification = std::holds_alternative<std::nullptr_t>(requestId);
			auto        future         = f(std::move(params));

			if(batchSender)
			{
				if(!isNotification)
				{
					auto responseWriter = batchSender->writeResponse(currentRequestId());
					responseWriter.writeData(
						[](std::string_view key, const auto& value, json::ObjectWriter& writer)
						{
							writeJson(key, value, writer);
						}, future.get());
				}
			}
			else
			{
				m_threadPool.addTask(
					[this, future = std::move(future), isNotification = isNotification, requestId]() mutable
					{
						handleAsyncResult<GenericMessage>(isNotification ? nullptr : &requestId, future);
					}
				);
			}
		}
	);

	return *this;
}

void MessageHandler::addPendingRequest(RequestResultPtr result, json::Integer id)
{
	const auto lock = std::lock_guard(m_pendingRequestsMutex);
	assert(!m_pendingRequests.contains(id));
	m_pendingRequests[id] = std::move(result);
}

MessageId MessageHandler::sendRequest(std::string_view method, const json::Value& params, GenericResponseCallback then, GenericErrorResponseCallback error)
{
	auto result = std::make_unique<CallbackRequestResult<json::Value, decltype(then), decltype(error)>>(
		std::move(then), std::move(error));
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	if(!params.isNull())
		requestSender.writeParams(params);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

FutureResponse<MessageHandler::GenericMessage> MessageHandler::sendRequest(std::string_view method, const json::Value& params)
{
	auto       result        = std::make_unique<FutureRequestResult<json::Value>>();
	auto       future        = result->future();
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	if(!params.isNull())
		requestSender.writeParams(params);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

void MessageHandler::sendNotification(std::string_view method, const json::Value& params)
{
	auto notificationSender = m_connection.notification(method);

	if(!params.isNull())
		notificationSender.writeParams(params);

	notificationSender.submit();
}

void MessageHandler::sendErrorResponse(const MessageId& messageId, int errorCode, std::string_view errorMessage, const std::optional<json::Value>& errorData )
{
	auto errorResponse = m_connection.errorResponse(messageId, errorCode, errorMessage);

	if(errorData.has_value())
		errorResponse.writeData(errorData.value());

	errorResponse.submit();
}

json::Integer MessageHandler::nextUniqueRequestId()
{
	static std::atomic<json::Integer> s_uniqueRequestId = 0;
	return ++s_uniqueRequestId;
}

} // namespace lsp
