#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <lsp/concepts.h>
#include <lsp/connection.h>
#include <lsp/error.h>
#include <lsp/jsonrpc/jsonrpc.h>
#include <lsp/messagebase.h>
#include <lsp/requestresult.h>
#include <lsp/serialization.h>
#include <lsp/threadpool.h>

namespace lsp{

using MessageId = jsonrpc::MessageId;

/*
 * MessageHandler
 */
class MessageHandler{
public:
	explicit MessageHandler(Connection connection, unsigned int maxResponseThreads = std::thread::hardware_concurrency() / 2);
	~MessageHandler() = default;

	void processNextMessage();
	void setConnection(Connection connection);

	// Only valid when called from within a request or response callback.
	// Throws std::logic_error if not called in that context.
	[[nodiscard]] static auto currentRequestId() -> const MessageId&;

	struct GenericMessage{
		using Params = json::Value;
		using Result = json::Value;
	};

	using GenericMessageCallback       = std::function<RequestResult<GenericMessage>(json::Value&&)>;
	using GenericResponseCallback      = std::function<void(json::Value&&)>;
	using GenericErrorResponseCallback = std::function<void(const ResponseError&)>;

	/*
	 * Callback registration
	 */

	template<typename M, typename F>
	requires IsRequestCallback<M, F> || IsNoParamsRequestCallback<M, F>
	auto on(F&& callback) -> MessageHandler&;

	template<typename M, typename F>
	requires IsNotificationCallback<M, F> || IsNoParamsNotificationCallback<M, F>
	auto on(F&& callback) -> MessageHandler&;

	auto on(std::string_view method, GenericMessageCallback callback) -> MessageHandler&;

	void remove(const std::string& method);

	/*
	 * sendRequest
	 */

	using ResponseErrorCallback = void(*)(const ResponseError&);

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires SendRequest<M, F, E>
	auto sendRequest(const typename M::Params& params, F&& then, E&& error = [](const ResponseError&){}) -> MessageId;

	template<typename M, typename F, typename E = ResponseErrorCallback>
	requires SendNoParamsRequest<M, F, E>
	auto sendRequest(F&& then, E&& error = [](const ResponseError&){}) -> MessageId;

	template<typename M>
	requires message::IsRequest<M> && message::HasParams<M>
	[[nodiscard]] auto sendRequest(const typename M::Params& params) -> FutureResponse<M>;

	template<typename M>
	requires message::IsRequest<M> && (!message::HasParams<M>)
	[[nodiscard]] auto sendRequest() -> FutureResponse<M>;

	auto sendRequest(std::string_view method, const json::Value& params = {}) -> FutureResponse<GenericMessage>;
	auto sendRequest(std::string_view method, const json::Value& params, GenericResponseCallback then, GenericErrorResponseCallback error) -> MessageId;
	void sendNotification(std::string_view method, const json::Value& params = {});

	/*
	 * sendNotification
	 */

	template<typename M>
	void sendNotification(const typename M::Params& params) requires SendNotification<M>;

	template<typename M>
	void sendNotification() requires SendNoParamsNotification<M>;

private:
	class ResponseResultBase;
	class RequestResultBase;
	using RequestResultPtr  = std::unique_ptr<RequestResultBase>;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using HandlerWrapper    = std::function<void(json::Value&&, Connection::BatchSender*)>;

	// General
	Connection                                       m_connection;
	ThreadPool                                       m_threadPool;
	// Incoming requests
	std::unordered_map<std::string, HandlerWrapper>  m_requestHandlersByMethod;
	std::mutex                                       m_requestHandlersMutex;
	// Outgoing requests
	std::mutex                                       m_pendingRequestsMutex;
	std::unordered_map<MessageId, RequestResultPtr>  m_pendingRequests;

	template<typename T>
	void sendResponse(const MessageId& messageId, const T& result, Connection::BatchSender* batchSender);

	template<typename M>
	void handleRequestResult(const MessageId* messageId, RequestResult<M>& result, Connection::BatchSender* batchSender);

	void processRequest(jsonrpc::Request&& request, Connection::BatchSender* batchSender);
	void processResponse(jsonrpc::Response&& response);
	void addHandler(std::string_view method, HandlerWrapper&& handlerFunc);
	void addPendingRequest(RequestResultPtr result, json::Integer id);
	void sendErrorResponse(
		const MessageId& messageId,
		int errorCode,
		std::string_view errorMessage,
		const std::optional<json::Value>& errorData,
		Connection::BatchSender* batchSender);

	static auto nextUniqueRequestId() -> json::Integer;

	/*
	 * Request result wrapper
	 */

	class RequestResultBase{
	public:
		virtual ~RequestResultBase() = default;
		virtual void setValueFromJson(json::Value&& json) = 0;
		virtual void setError(ResponseError&& error) = 0;
	};

	template<typename T, typename F, typename E>
	class CallbackRequestResult final : public RequestResultBase{
	public:
		CallbackRequestResult(F&& then, E&& error)
			: m_then{std::forward<F>(then)}
			, m_error{std::forward<E>(error)}
		{
		}

		void setValueFromJson(json::Value&& json) override;
		void setError(ResponseError&& error) override;

	private:
		F m_then;
		E m_error;
	};

	template<typename T>
	class FutureRequestResult final : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(json::Value&& json) override;
		void setError(ResponseError&& error) override;

	private:
		std::promise<T> m_promise;
	};
};

} // namespace lsp

#include "messagehandler.inl"
