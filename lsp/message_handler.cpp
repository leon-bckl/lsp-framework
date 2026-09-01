#include <cassert>
#include <lsp/message_handler.h>

namespace lsp{
namespace{

thread_local const MessageHandler::RequestContext* t_requestContext = nullptr;

} // namespace

/*
 * MessageHandler::RequestContext
 */

MessageHandler::RequestContext::RequestContext(MessageHandler& messageHandler, RequestId requestId)
	: m_messageHandler{&messageHandler}
	, m_requestId{std::move(requestId)}
{
	assert(!t_requestContext);
	t_requestContext = this;
}

MessageHandler::RequestContext::~RequestContext()
{
	if(t_requestContext == this)
		t_requestContext = nullptr;
}

auto MessageHandler::RequestContext::get() -> const RequestContext&
{
	if(!t_requestContext)
		throw std::logic_error("RequestContext::get called outside of a request context");

	return *t_requestContext;
}

auto MessageHandler::RequestContext::tryGet() -> const RequestContext*
{
	return t_requestContext;
}

/*
 * MessageHandler
 */

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
		try
		{
			lock.unlock();

			if(request.isNotification())
			{
				handlerIt->second(
					request.params.has_value() ? std::move(*request.params) : json::Null{},
					nullptr,
					nullptr);
			}
			else
			{
				handlerIt->second(
					request.params.has_value() ? std::move(*request.params) : json::Null{},
					&*request.id,
					batchSender);
			}
		}
		catch(const RequestError& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, e.code(), e.what(), e.data(), batchSender);
		}
		catch(const json::TypeError& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, MessageError::InvalidParams, e.what(), {}, batchSender);
		}
		catch(const std::exception& e)
		{
			if(!request.isNotification())
				sendErrorResponse(*request.id, MessageError::InternalError, e.what(), {}, batchSender);
		}
	}
	else
	{
		if(!request.isNotification())
			sendErrorResponse(*request.id, MessageError::MethodNotFound, "Method not found", {}, nullptr);
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

	const auto requestContext = RequestContext(*this, response.id);

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

void MessageHandler::addHandler(std::string_view method, HandlerWrapper&& handlerFunc)
{
	std::lock_guard lock{m_requestHandlersMutex};
	m_requestHandlersByMethod[std::string(method)] = std::move(handlerFunc);
}

void MessageHandler::addPendingRequest(RequestResultPtr result, json::Integer id)
{
	const auto lock = std::lock_guard(m_pendingRequestsMutex);
	assert(!m_pendingRequests.contains(id));
	m_pendingRequests[id] = std::move(result);
}

void MessageHandler::sendNotification(std::string_view method, const json::Value& params)
{
	auto notificationSender = m_connection.notification(method);

	if(!params.isNull())
		notificationSender.writeParams(params);

	notificationSender.submit();
}

void MessageHandler::sendErrorResponse(
	const MessageId& messageId,
	int errorCode,
	std::string_view errorMessage,
	const std::optional<json::Value>& errorData,
	Connection::BatchSender* batchSender)
{
	if(batchSender)
	{
		auto responseWriter = batchSender->writeError(messageId, errorCode, errorMessage);

		if(errorData.has_value())
		{
			responseWriter.writeData(
				[](std::string_view key, const json::Value& value, json::ObjectWriter& objectWriter)
				{
					objectWriter.write(key, value);
				}, *errorData);
		}
	}
	else
	{
		auto errorResponse = m_connection.errorResponse(messageId, errorCode, errorMessage);

		if(errorData.has_value())
			errorResponse.writeData(*errorData);

		errorResponse.submit();
	}
}

auto MessageHandler::nextUniqueRequestId() -> json::Integer
{
	static std::atomic<json::Integer> s_uniqueRequestId = 0;
	return ++s_uniqueRequestId;
}

} // namespace lsp
