#include "messagedispatcher.h"

#include <cassert>
#include "error.h"

namespace lsp{
namespace{
	json::Integer nextUniqueRequestId()
	{
		static std::atomic<json::Integer> g_uniqueRequestId = 0;
		return ++g_uniqueRequestId;
	}
}

MessageDispatcher::MessageDispatcher(Connection& connection)
	: m_connection{connection}
{
}

void MessageDispatcher::onResponse(jsonrpc::Response&& response)
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
		result->setException(std::make_exception_ptr(ResponseError{ErrorCodes{error.code}, error.message, error.data}));
	}
}

void MessageDispatcher::onResponseBatch(jsonrpc::ResponseBatch&& batch)
{
	// This should never be called as no batches are ever sent
	for(auto&& r : batch)
		onResponse(std::move(r));
}

jsonrpc::MessageId MessageDispatcher::sendRequest(MessageMethod method, RequestResultPtr result, const std::optional<json::Any>& params)
{
	std::lock_guard lock{m_pendingRequestsMutex};
	auto messageId = nextUniqueRequestId();
	m_pendingRequests[messageId] = std::move(result);
	auto methodStr = messageMethodToString(method);
	m_connection.sendRequest(jsonrpc::createRequest(messageId, methodStr, params));
	return messageId;
}

void MessageDispatcher::sendNotification(MessageMethod method, const std::optional<json::Any>& params)
{
	const auto methodStr = messageMethodToString(method);
	auto notification = jsonrpc::createNotification(methodStr, params);
	m_connection.sendRequest(std::move(notification));
}

} // namespace lsp
