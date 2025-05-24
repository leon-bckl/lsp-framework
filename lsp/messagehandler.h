#pragma once

#include <mutex>
#include <future>
#include <utility>
#include <concepts>
#include <functional>
#include <lsp/error.h>
#include <lsp/strmap.h>
#include <lsp/connection.h>
#include <lsp/threadpool.h>
#include <lsp/messagebase.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

class ErrorCodes;
class Connection;

using MessageId = jsonrpc::MessageId;

/*
 * The result returned from a request handler callback that does processing asynchronously
 */
template<typename MessageType>
using AsyncRequestResult = std::future<typename MessageType::Result>;

/*
 * Concepts to verify the type of callback
 */

template<typename MessageType, typename F>
concept IsRequestCallbackResult =
	std::same_as<
		std::invoke_result_t<F, MessageId, typename MessageType::Params>,
		typename MessageType::Result> ||
	std::same_as<
		std::invoke_result_t<F, MessageId, typename MessageType::Params>,
		AsyncRequestResult<MessageType>>;

template<typename MessageType, typename F>
concept IsNoParamsRequestCallbackResult =
	std::same_as<
		std::invoke_result_t<F, MessageId>,
		typename MessageType::Result> ||
	std::same_as<
		std::invoke_result_t<F, MessageId>,
		AsyncRequestResult<MessageType>>;

template<typename MessageType, typename F>
concept IsNotificationCallbackResult =
	std::same_as<
		std::invoke_result_t<F, typename MessageType::Params>, void>;

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallbackResult =
	std::same_as<
		std::invoke_result_t<F>, void>;

template<typename MessageType, typename F>
concept IsRequestCallback = message::HasParams<MessageType> &&
                            message::HasResult<MessageType> &&
                            std::invocable<F, const MessageId&, typename MessageType::Params&&> &&
                            IsRequestCallbackResult<MessageType, F>;

template<typename MessageType, typename F>
concept IsNoParamsRequestCallback = !message::HasParams<MessageType> &&
                                    message::HasResult<MessageType> &&
                                    std::invocable<F, const MessageId&> &&
                                    IsNoParamsRequestCallbackResult<MessageType, F>;

template<typename MessageType, typename F>
concept IsNotificationCallback = message::HasParams<MessageType> &&
                                 !message::HasResult<MessageType> &&
                                 std::invocable<F, typename MessageType::Params&&> &&
                                 IsNotificationCallbackResult<MessageType, F>;

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallback = !message::HasParams<MessageType> &&
                                         !message::HasResult<MessageType> &&
                                         std::invocable<F> &&
                                         IsNoParamsNotificationCallbackResult<MessageType, F>;

/*
 * The return type of MessageHandler::sendRequest.
 * id can be used to send a cancel notification (if the requests supports it).
 * result will contain the result of the request once it is ready.
 * Do not call result.wait() on the same thread that handles incoming messages as that would result in infinte waiting.
 */
template<typename MessageType>
struct FutureResponse{
	using ResultFuture = std::future<typename MessageType::Result>;

	FutureResponse(MessageId _messageId, ResultFuture _result)
		: messageId{std::move(_messageId)},
		  result(std::move(_result))
	{
	}

	MessageId    messageId;
	ResultFuture result;
};

/**
 * The MessageHandler class is used to send and receive requests and notifications.
 * A new thread is spawned that waits for the results of async requests and sends the responses.
 *
 * Callbacks for the different message types can be registered using the 'add' method.
 * To avoid copying too much data, request parameters are passed as an rvalue reference.
 * A callback is any callable object that matches the return and parameter types of the message.
 * The first parameter of all request callbacks should be the id 'const MessageId&'. Notification callbacks don't have an id parameter and don't return a value.
 * Here's a callback for the Initialize request:
 *
 * messageHandler.add<lsp::requests::Initialize>([](const MessageId& id, lsp::requests::Initialize::Params&& params)
 * {
 *    lsp::requests::Initialize::Result result;
 *    // Initialize the result and return it or throw an lsp::RequestError if there was a problem
 *    // Alternatively do processing asynchronously and return a std::future here
 *    return result;
 * });
 */
class MessageHandler{
public:
	explicit MessageHandler(Connection& connection, unsigned int maxResponseThreads = std::thread::hardware_concurrency() / 2);
	~MessageHandler() = default;

	void processIncomingMessages();

	/*
	 * Callback registration
	 */

	template<typename MessageType, typename F>
	requires IsRequestCallback<MessageType, F>
	MessageHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNoParamsRequestCallback<MessageType, F>
	MessageHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNotificationCallback<MessageType, F>
	MessageHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNoParamsNotificationCallback<MessageType, F>
	MessageHandler& add(F&& handlerFunc);

	void remove(std::string_view method);

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
	MessageId sendRequest(typename MessageType::Params&& params, F&& then, E&& error = [](const Error&){});

	template<typename MessageType, typename F, typename E = void(*)(const Error&)>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) &&
	         std::invocable<F, typename MessageType::Result&&> &&
	         std::invocable<E, const Error&>
	MessageId sendRequest(F&& then, E&& error = [](const Error&){});

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
	class ResponseResultBase;
	class RequestResultBase;
	using RequestResultPtr = std::unique_ptr<RequestResultBase>;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using OptionalResponse = std::optional<jsonrpc::Response>;
	using HandlerWrapper = std::function<OptionalResponse(const MessageId&, json::Any&&, bool)>;

	// General
	Connection&                                      m_connection;
	// Incoming requests
	StrMap<std::string, HandlerWrapper>              m_requestHandlersByMethod;
	std::mutex                                       m_requestHandlersMutex;
	ThreadPool                                       m_threadPool;
	// Outgoing requests
	std::mutex                                       m_pendingRequestsMutex;
	std::unordered_map<MessageId, RequestResultPtr>  m_pendingRequests;

	template<typename T>
	static jsonrpc::Response createResponse(const MessageId& id, T&& result)
	{
		return jsonrpc::createResponse(id, toJson(std::forward<T>(result)));
	}

	template<typename MessageType>
	jsonrpc::Response createResponseFromAsyncResult(MessageId&& id, AsyncRequestResult<MessageType>& result)
	{
		try
		{
			return MessageHandler::createResponse(std::move(id), result.get());
		}
		catch(const RequestError& e)
		{
			return jsonrpc::createErrorResponse(std::move(id), e.code(), e.what());
		}
		catch(std::exception& e)
		{
			return jsonrpc::createErrorResponse(std::move(id), jsonrpc::Error::InternalError, e.what());
		}
	}

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

	template<typename T>
	class FutureRequestResult : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(json::Any&& json) override
		{
			T value{};
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
			T value{};
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

} // namespace lsp

#include "messagehandler.inl"
