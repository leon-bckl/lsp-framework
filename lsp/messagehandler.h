#pragma once

#include <mutex>
#include <future>
#include <thread>
#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include <unordered_map>
#include <lsp/error.h>
#include <lsp/connection.h>
#include <lsp/messagebase.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

class ErrorCodes;
class Connection;

/*
 * The result returned from a request handler callback that does processing asynchronously
 */
template<typename MessageType>
using AsyncRequestResult = std::future<typename MessageType::Result>;

/*
 * Concepts to verify the type of callback
 */

template<typename MessageType, typename F>
concept IsRequestCallback = message::HasParams<MessageType> &&
                            message::HasResult<MessageType> &&
                            std::invocable<F, const jsonrpc::MessageId&, typename MessageType::Params&&>;

template<typename MessageType, typename F>
concept IsNoParamsRequestCallback = !message::HasParams<MessageType> &&
                                    message::HasResult<MessageType> &&
                                    std::invocable<F, const jsonrpc::MessageId&>;

template<typename MessageType, typename F>
concept IsNotificationCallback = message::HasParams<MessageType> &&
							  						     !message::HasResult<MessageType> &&
							  						     std::invocable<F, typename MessageType::Params&&>;

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallback = !message::HasParams<MessageType> &&
                                         !message::HasResult<MessageType> &&
                                         std::invocable<F>;

/*
 * The return type of MessageHandler::sendRequest.
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

/**
 * The MessageHandler class is used to send and receive requests and notifications.
 * A new thread is spawned that waits for the results of async requests and sends the responses.
 *
 * Callbacks for the different message types can be registered using the 'add' method.
 * To avoid copying too much data, request parameters are passed as an rvalue reference.
 * A callback is any callable object that matches the return and parameter types of the message.
 * The first parameter of all request callbacks should be the id 'const jsonrpc::MessageId&'. Notification callbacks don't have an id parameter and don't return a value.
 * Here's a callback for the Initialize request:
 *
 * messageHandler.add<lsp::requests::Initialize>([](const jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
 * {
 *    lsp::requests::Initialize::Result result;
 *    // Initialize the result and return it or throw an lsp::RequestError if there was a problem
 *    // Alternatively do processing asynchronously and return a std::future here
 *    return result;
 * });
 */
class MessageHandler : public RequestHandlerInterface, ResponseHandlerInterface{
public:
	MessageHandler(Connection& connection);
	~MessageHandler();

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

	void remove(MessageMethod method);

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
	class ResponseResultBase;
	class RequestResultBase;
	using RequestResultPtr = std::unique_ptr<RequestResultBase>;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using OptionalResponse = std::optional<jsonrpc::Response>;
	using HandlerWrapper = std::function<OptionalResponse(const jsonrpc::MessageId&, json::Any&&, bool)>;

	// General
	Connection&                                               m_connection;
	// Incoming requests
	std::vector<HandlerWrapper>                               m_requestHandlers;
	std::mutex                                                m_requestHandlersMutex;
	bool                                                      m_running = true;
	std::thread                                               m_asyncResponseThread;
	std::mutex                                                m_pendingResponsesMutex;
	std::unordered_map<jsonrpc::MessageId, ResponseResultPtr> m_pendingResponses;
	// Outgoing requests
	std::mutex                                                m_pendingRequestsMutex;
	std::unordered_map<jsonrpc::MessageId, RequestResultPtr>  m_pendingRequests;

	void onRequest(jsonrpc::Request&& request) override;
	void onRequestBatch(jsonrpc::RequestBatch&& batch) override;

	OptionalResponse processRequest(jsonrpc::Request&& request, bool allowAsync);
	void addHandler(MessageMethod method, HandlerWrapper&& handlerFunc);
	void addResponseResult(const jsonrpc::MessageId& id, ResponseResultPtr result);

	template<typename T>
	static jsonrpc::Response createResponse(const jsonrpc::MessageId& id, T&& result)
	{
		return jsonrpc::createResponse(id, toJson(std::forward<T>(result)));
	}

	/*
	 * Response result wrapper
	 */

	class ResponseResultBase{
	public:
		virtual ~ResponseResultBase() = default;
		virtual bool isReady() const = 0;
		virtual jsonrpc::Response createResponse() = 0;
	};

	template<typename T>
	class ResponseResult : public ResponseResultBase{
	public:
		ResponseResult(jsonrpc::MessageId id, std::future<T> future);

		bool isReady() const override;
		jsonrpc::Response createResponse() override;

	private:
		jsonrpc::MessageId m_id;
		std::future<T>     m_future;
	};

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

/*
 * RequestHandler::ResponseResult
 */

template<typename T>
MessageHandler::ResponseResult<T>::ResponseResult(jsonrpc::MessageId id, std::future<T> future)
	: m_id{std::move(id)}
	, m_future{std::move(future)}
{
	if(!m_future.valid())
		throw RequestError{ErrorCodes::InternalError, "Request handler returned invalid result"};
}

template<typename T>
bool MessageHandler::ResponseResult<T>::isReady() const
{
	const auto status = m_future.wait_for(std::chrono::seconds{0});

	if(status == std::future_status::deferred)
	{
		m_future.wait();
		return true;
	}

	return status == std::future_status::ready;
}

template<typename T>
jsonrpc::Response MessageHandler::ResponseResult<T>::createResponse()
{
	assert(isReady());
	try
	{
		return MessageHandler::createResponse(m_id, m_future.get());
	}
	catch(const RequestError& e)
	{
		return jsonrpc::createErrorResponse(m_id, e.code(), e.what());
	}
	catch(std::exception& e)
	{
		return jsonrpc::createErrorResponse(m_id, ErrorCodes{ErrorCodes::InternalError}, e.what());
	}
}

/*
 * RequestHandler::add
 */

template<typename MessageType, typename F>
requires IsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, json::Any&& json, bool allowAsync) -> OptionalResponse
	{
		typename MessageType::Params params;
		fromJson(std::move(json), params);

		if constexpr(std::same_as<
			             std::invoke_result_t<F, jsonrpc::MessageId, typename MessageType::Params>,
			             AsyncRequestResult<MessageType>
		             >)
		{
			auto result = f(id, std::move(params));

			if(allowAsync)
			{
				addResponseResult(id, std::make_unique<ResponseResult<typename MessageType::Result>>(id, std::move(result)));
				return std::nullopt;
			}

			return createResponse(id, result.get());
		}
		else
		{
			(void)this;
			(void)allowAsync;
			return createResponse(id, f(id, std::move(params)));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, json::Any&&, bool allowAsync) -> OptionalResponse
	{
		if constexpr(std::same_as<
			             std::invoke_result_t<F, jsonrpc::MessageId>,
			             AsyncRequestResult<MessageType>
		             >)
		{
			auto result = f(id);

			if(allowAsync)
			{
				addResponseResult(id, std::make_unique<ResponseResult<typename MessageType::Result>>(std::move(result)));
				return std::nullopt;
			}

			return createResponse(id, result.get());
		}
		else
		{
			(void)this;
			(void)allowAsync;
			return createResponse(id, f(id));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNotificationCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId&, json::Any&& json, bool) -> OptionalResponse
	{
		typename MessageType::Params params;
		fromJson(std::move(json), params);
		f(std::move(params));
		return std::nullopt;
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsNotificationCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId&, json::Any&&, bool) -> OptionalResponse
	{
		f();
		return std::nullopt;
	});

	return *this;
}

template<typename MessageType>
requires message::HasParams<MessageType> && message::HasResult<MessageType>
FutureResponse<MessageType> MessageHandler::sendRequest(typename MessageType::Params&& params)
{
	auto result    = std::make_unique<FutureRequestResult<typename MessageType::Result>>();
	auto future    = result->future();
	auto messageId = sendRequest(MessageType::Method, std::move(result), toJson(std::move(params)));
	return {std::move(messageId), std::move(future)};
}

template<typename MessageType>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
FutureResponse<MessageType> MessageHandler::sendRequest()
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
jsonrpc::MessageId MessageHandler::sendRequest(typename MessageType::Params&& params, F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result), toJson(std::move(params)));
}

template<typename MessageType, typename F, typename E>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) &&
         std::invocable<F, typename MessageType::Result&&> &&
         std::invocable<E, const Error&>
jsonrpc::MessageId MessageHandler::sendRequest(F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result));
}

} // namespace lsp
