#pragma once

#include <mutex>
#include <future>
#include <lsp/connection.h>
#include <lsp/messagebase.h>
#include "error.h"

namespace lsp{

/*
 * The return type of MessageDispatcher::sendRequest.
 * id can be used to send a cancel notification (if the requests supports it).
 * result will contain the result of the request once it is ready.
 * Do not call result.wait() on the same thread that handles incoming messages as that would result in infinte waiting.
 */
template<typename MessageType>
struct FutureResponse{
	using ResultFuture = std::future<typename MessageType::Result>;

	FutureResponse(jsonrpc::MessageId _messageId, ResultFuture _result)
		: messageId{std::move(_messageId)},
		  result(std::move(_result))
	{
	}

	jsonrpc::MessageId messageId;
	ResultFuture       result;
};

/*
 * The MessageDispatcher is responsible for sending outgoing requests and notifications.
 * If an incoming response is received, it associates it with a previosly sent request and updates the future result in the response.
 */
class MessageDispatcher : public ResponseHandlerInterface{
public:
	MessageDispatcher(Connection& connection);

	/*
	 * sendRequest
	 */

	template<typename MessageType>
	requires message::HasParams<MessageType> && message::HasResult<MessageType>
	[[nodiscard]] FutureResponse<MessageType> sendRequest(typename MessageType::Params&& params);

	template<typename MessageType>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
	[[nodiscard]] FutureResponse<MessageType> sendRequest();

	template<typename MessageType, typename F, typename E = void(*)(const Error&)>
	requires message::HasParams<MessageType> && message::HasResult<MessageType> &&
	         std::invocable<F, typename MessageType::Result&&> &&
	         std::invocable<E, const Error&>
	jsonrpc::MessageId sendRequest(typename MessageType::Params&& params, F&& then, E&& error = [](const Error&){});

	template<typename MessageType, typename F, typename E = void(*)(const Error&)>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) &&
	         std::invocable<F, typename MessageType::Result&&> &&
	         std::invocable<E, const Error&>
	jsonrpc::MessageId sendRequest(F&& then, E&& error = [](const Error&){});

	/*
	 * sendNotification
	 */

	template<typename MessageType>
	requires (!message::HasParams<MessageType>) && (!message::HasResult<MessageType>)
	void sendNotification()
	{
		sendNotification(MessageType::Method);
	}

	template<typename MessageType>
	requires message::HasParams<MessageType> && (!message::HasResult<MessageType>)
	void sendNotification(typename MessageType::Params&& params)
	{
		sendNotification(MessageType::Method, toJson(std::move(params)));
	}

private:
	class RequestResultBase;
	using RequestResultPtr = std::unique_ptr<RequestResultBase>;

	Connection&                                               m_connection;
	std::mutex                                                m_pendingRequestsMutex;
	std::unordered_map<jsonrpc::MessageId, RequestResultPtr>  m_pendingRequests;

	void onResponse(jsonrpc::Response&& response) override;
	void onResponseBatch(jsonrpc::ResponseBatch&& batch) override;

	jsonrpc::MessageId sendRequest(MessageMethod method, RequestResultPtr result, const std::optional<json::Any>& params = std::nullopt);
	void sendNotification(MessageMethod method, const std::optional<json::Any>& params = std::nullopt);

	/*
	 * Request result wrapper
	 */

	class RequestResultBase{
	public:
		virtual ~RequestResultBase() = default;
		virtual void setValueFromJson(json::Any&& json) = 0;
		virtual void setException(std::exception_ptr e) = 0;
	};

	template<typename T>
	class FutureRequestResult : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(json::Any&& json) override
		{
			T value;
			fromJson(std::move(json), value);
			m_promise.set_value(std::move(value));
		}

		void setException(std::exception_ptr e) override
		{
			m_promise.set_exception(e);
		}

	private:
		std::promise<T> m_promise;
	};

	template<typename T, typename F, typename E>
	class CallbackRequestResult : public RequestResultBase{
	public:
		CallbackRequestResult(F&& then, E&& error)
			: m_then{std::forward<F>(then)}
			, m_error{std::forward<E>(error)}
		{
		}

		void setValueFromJson(json::Any&& json) override
		{
			T value;
			fromJson(std::move(json), value);
			m_then(std::move(value));
		}

		void setException(std::exception_ptr e) override
		{
			try
			{
				std::rethrow_exception(e);
			}
			catch(Error& error)
			{
				m_error(error);
			}
		}

	private:
		F m_then;
		E m_error;
	};
};

template<typename MessageType>
requires message::HasParams<MessageType> && message::HasResult<MessageType>
FutureResponse<MessageType> MessageDispatcher::sendRequest(typename MessageType::Params&& params)
{
	auto result    = std::make_unique<FutureRequestResult<typename MessageType::Result>>();
	auto future    = result->future();
	auto messageId = sendRequest(MessageType::Method, std::move(result), toJson(std::move(params)));
	return {std::move(messageId), std::move(future)};
}

template<typename MessageType>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
FutureResponse<MessageType> MessageDispatcher::sendRequest()
{
	auto result    = std::make_unique<FutureRequestResult<typename MessageType::Result>>();
	auto future    = result->future();
	auto messageId = sendRequest(MessageType::Method, std::move(result));
	return {std::move(messageId), std::move(future)};
}

template<typename MessageType, typename F, typename E>
requires message::HasParams<MessageType> && message::HasResult<MessageType> &&
         std::invocable<F, typename MessageType::Result&&> &&
         std::invocable<E, const Error&>
jsonrpc::MessageId MessageDispatcher::sendRequest(typename MessageType::Params&& params, F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result), toJson(std::move(params)));
}

template<typename MessageType, typename F, typename E>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) &&
         std::invocable<F, typename MessageType::Result&&> &&
         std::invocable<E, const Error&>
jsonrpc::MessageId MessageDispatcher::sendRequest(F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result));
}

} // namespace lsp
