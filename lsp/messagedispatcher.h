#pragma once

#include <mutex>
#include <lsp/connection.h>
#include <lsp/messagebase.h>

namespace lsp{

class MessageDispatcher : public ResponseHandlerInterface{
public:
	MessageDispatcher(Connection& connection);

	/*
	 * sendRequest
	 */

	template<typename MessageType>
	requires message::HasParams<MessageType> && message::HasResult<MessageType>
	[[nodiscard]] ASyncRequestResult<MessageType> sendRequest(const typename MessageType::Params& params);

	template<typename MessageType>
	requires message::HasParams<MessageType> && message::HasResult<MessageType> && message::HasPartialResult<MessageType>
	[[nodiscard]] ASyncRequestResult<MessageType> sendRequest(const typename MessageType::Params& params);

	template<typename MessageType>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
	[[nodiscard]] ASyncRequestResult<MessageType> sendRequest();

	template<typename MessageType>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) && (!message::HasPartialResult<MessageType>)
	[[nodiscard]] ASyncRequestResult<MessageType> sendRequest();

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
	void sendNotification(const typename MessageType::Params& params)
	{
		sendNotification(MessageType::Method, toJson(params));
	}

private:
	class RequestResultBase;
	using RequestResultPtr = std::unique_ptr<RequestResultBase>;

	Connection&                                               m_connection;

	std::mutex                                                m_pendingRequestsMutex;
	std::unordered_map<jsonrpc::MessageId, RequestResultPtr>  m_pendingRequests;

	static std::atomic<json::Integer>                         s_uniqueRequestId;

	void onResponse(jsonrpc::Response&& response) override;
	void onResponseBatch(jsonrpc::ResponseBatch&& batch) override;

	void sendRequest(MessageMethod method, RequestResultPtr result, const std::optional<json::Any>& params = std::nullopt);
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
	class RequestResult : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(json::Any&& json) override
		{
			T value;
			fromJson(json, std::move(value));
			m_promise.set_value(std::move(value));
		}

		void setException(std::exception_ptr e) override
		{
			m_promise.set_exception(e);
		}

	private:
		std::promise<T> m_promise;
	};
};

template<typename MessageType>
requires message::HasParams<MessageType> && message::HasResult<MessageType>
ASyncRequestResult<MessageType> MessageDispatcher::sendRequest(const typename MessageType::Params& params)
{
	auto result = std::make_unique<RequestResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result), toJson(params));
	return future;
}

template<typename MessageType>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
ASyncRequestResult<MessageType> MessageDispatcher::sendRequest()
{
	auto result = std::make_unique<RequestResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result));
	return future;
}

} // namespace lsp
