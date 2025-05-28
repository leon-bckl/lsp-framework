#pragma once

#include <mutex>
#include <future>
#include <utility>
#include <functional>
#include <lsp/error.h>
#include <lsp/strmap.h>
#include <lsp/concepts.h>
#include <lsp/connection.h>
#include <lsp/threadpool.h>
#include <lsp/messagebase.h>
#include <lsp/requestresult.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

using MessageId = jsonrpc::MessageId;

/*
 * MessageHandler
 */
class MessageHandler{
public:
	explicit MessageHandler(Connection& connection, unsigned int maxResponseThreads = std::thread::hardware_concurrency() / 2);
	~MessageHandler() = default;

	void processIncomingMessages();
	// Only valid when called from within a request or response callback.
	// Throws std::logic_error if not called in that context.
	[[nodiscard]] static const MessageId& currentRequestId();

	/*
	 * Callback registration
	 */

	template<typename M, typename F>
	MessageHandler& add(F&& handlerFunc) requires IsRequestCallback<M, F>;

	template<typename M, typename F>
	MessageHandler& add(F&& handlerFunc) requires IsNoParamsRequestCallback<M, F>;

	template<typename M, typename F>
	MessageHandler& add(F&& handlerFunc) requires IsNotificationCallback<M, F>;

	template<typename M, typename F>
	MessageHandler& add(F&& handlerFunc) requires IsNoParamsNotificationCallback<M, F>;

	void remove(std::string_view method);

	/*
	 * sendRequest
	 */

	using ResponseErrorCallback = void(*)(const ResponseError&);

	template<typename M, typename F, typename E = ResponseErrorCallback>
	MessageId sendRequest(typename M::Params&& params, F&& then, E&& error = [](const ResponseError&){}) requires SendRequest<M, F, E>;

	template<typename M, typename F, typename E = ResponseErrorCallback>
	MessageId sendRequest(F&& then, E&& error = [](const ResponseError&){}) requires SendNoParamsRequest<M, F, E>;

	template<typename M>
	[[nodiscard]] FutureResponse<M> sendRequest(typename M::Params&& params) requires message::IsRequest<M> && message::HasParams<M>;

	template<typename M>
	[[nodiscard]] FutureResponse<M> sendRequest() requires message::IsRequest<M> && (!message::HasParams<M>);

	/*
	 * sendNotification
	 */

	template<typename M>
	void sendNotification(typename M::Params&& params) requires SendNotification<M>;

	template<typename M>
	void sendNotification() requires SendNoParamsNotification<M>;

private:
	class ResponseResultBase;
	class RequestResultBase;
	using RequestResultPtr  = std::unique_ptr<RequestResultBase>;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using OptionalResponse  = std::optional<jsonrpc::Response>;
	using HandlerWrapper    = std::function<OptionalResponse(json::Any&&, bool)>;

	// General
	Connection&                                      m_connection;
	ThreadPool                                       m_threadPool;
	// Incoming requests
	StrMap<std::string, HandlerWrapper>              m_requestHandlersByMethod;
	std::mutex                                       m_requestHandlersMutex;
	// Outgoing requests
	std::mutex                                       m_pendingRequestsMutex;
	std::unordered_map<MessageId, RequestResultPtr>  m_pendingRequests;

	template<typename T>
	static jsonrpc::Response createResponse(const MessageId& id, T&& result);

	template<typename M>
	static jsonrpc::Response createResponseFromAsyncResult(const MessageId& id, AsyncRequestResult<M>& result);

	OptionalResponse processRequest(jsonrpc::Request&& request, bool allowAsync);
	void addHandler(std::string_view method, HandlerWrapper&& handlerFunc);
	void sendResponse(jsonrpc::Response&& response);
	void processResponse(jsonrpc::Response&& response);
	MessageId sendRequest(std::string_view method, RequestResultPtr result, const std::optional<json::Any>& params = std::nullopt);
	void sendNotification(std::string_view method, const std::optional<json::Any>& params = std::nullopt);

	/*
	 * Request result wrapper
	 */

	class RequestResultBase{
	public:
		virtual ~RequestResultBase() = default;
		virtual void setValueFromJson(json::Any&& json) = 0;
		virtual void setException(std::exception_ptr e) = 0;
	};

	template<typename T, typename F, typename E>
	class CallbackRequestResult final : public RequestResultBase{
	public:
		CallbackRequestResult(F&& then, E&& error)
			: m_then{std::forward<F>(then)}
			, m_error{std::forward<E>(error)}
		{
		}

		void setValueFromJson(json::Any&& json) override;
		void setException(std::exception_ptr e) override;

	private:
		F m_then;
		E m_error;
	};

	template<typename T>
	class FutureRequestResult final : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(json::Any&& json) override;
		void setException(std::exception_ptr e) override;

	private:
		std::promise<T> m_promise;
	};
};

} // namespace lsp

#include "messagehandler.inl"
