#pragma once

#include <future>
#include <variant>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

using MessageId = jsonrpc::MessageId;

/*
 * The result returned from a request handler callback that does processing asynchronously
 */
template<typename MessageType>
using AsyncRequestResult = std::future<typename MessageType::Result>;

using AsyncNotificationResult = std::future<void>;

/*
 * The return type of MessageHandler::sendRequest.
 * id can be used to send a cancel notification (if the request supports it).
 * result will contain the result of the request once it is ready.
 * Do not call result.wait() on the same thread that handles incoming messages as that would result in infinte waiting.
 */
template<typename MessageType>
struct FutureResponse{
	using ResultFuture = std::future<typename MessageType::Result>;

	FutureResponse(MessageId _messageId, ResultFuture _result)
		: messageId{std::move(_messageId)},
		  result{std::move(_result)}
	{
	}

	MessageId    messageId;
	ResultFuture result;
};

template<typename MessageType>
class RequestResult{
public:
	using FutureType = std::future<typename MessageType::Result>;

	RequestResult(typename MessageType::Result&& result)
		: m_result{std::move(result)}
	{
	}

	RequestResult(FutureType&& future)
		: m_result{std::move(future)}
	{
	}

	[[nodiscard]] auto requestId() const -> const MessageId&{ return m_requestId; }
	[[nodiscard]] auto isAsync() const -> bool{ return std::holds_alternative<FutureType>(m_result); }

	[[nodiscard]] auto get() -> typename MessageType::Result
	{
		if(auto* future = std::get_if<FutureType>(&m_result))
			return future->get();

		return std::move(std::get<typename MessageType::Result>(m_result));
	}

private:
	std::variant<typename MessageType::Result, FutureType> m_result;
	MessageId                                              m_requestId;
};

} // namespace lsp
