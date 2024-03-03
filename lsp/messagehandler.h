#pragma once

#include <mutex>
#include <atomic>
#include <future>
#include <thread>
#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <lsp/messagebase.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

class ErrorCodes;
class Connection;

template<typename MessageType>
using ASyncRequestResult = std::future<typename MessageType::Result>;

template<typename MessageType, typename F>
concept IsRequestCallback = (message::HasParams<MessageType> &&
                             message::HasResult<MessageType> &&
                             (std::convertible_to<F, std::function<typename MessageType::Result(const jsonrpc::MessageId&, typename MessageType::Params&&)>> ||
                              std::convertible_to<F, std::function<ASyncRequestResult<MessageType>(const jsonrpc::MessageId&, typename MessageType::Params&&)>>));

template<typename MessageType, typename F>
concept IsNoParamsRequestCallback = ((!message::HasParams<MessageType>) &&
                                     message::HasResult<MessageType> &&
                                     (std::convertible_to<F, std::function<typename MessageType::Result(const jsonrpc::MessageId&)>> ||
                                      std::convertible_to<F, std::function<ASyncRequestResult<MessageType>(const jsonrpc::MessageId&, typename MessageType::Params&&)>>));

template<typename MessageType, typename F>
concept IsNotificationCallback = (message::HasParams<MessageType> &&
							  						      (!message::HasResult<MessageType>) &&
							  						      std::convertible_to<F, std::function<void(typename MessageType::Params&&)>>);

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallback = ((!message::HasParams<MessageType>) &&
                                          (!message::HasResult<MessageType>) &&
                                          std::convertible_to<F, std::function<void()>>);

/**
 * The MessageHandler class is used to send and receive requests and notifications.
 * 'processIncomingMessages' reads all messages from the connection and triggers callbacks or updates request results.
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
 *
 * There are convenience typedefs for the message return and parameter types:
 *  - MessageType::Result
 *  - MessageType::Params
 *
 * The 'sendRequest' function returns a std::future for the result type.
 * Make sure not to call std::future::wait on the same thread that calls 'processIncomingMessages', because it would block.
 */
class MessageHandler{
public:
	MessageHandler(Connection& connection);
	~MessageHandler();

	/*
	 * Message processing
	 */

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

	bool handlesRequest(MessageMethod method);

private:
	class RequestResultBase;
	class ResponseResultBase;
	using RequestResultPtr = std::unique_ptr<RequestResultBase>;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using HandlerWrapper = std::function<std::optional<jsonrpc::Response>(const jsonrpc::MessageId&, const json::Any&)>;

	Connection&                                               m_connection;
	std::vector<HandlerWrapper>                               m_requestHandlers;

	bool                                                      m_running = true;
	std::thread                                               m_responseThread;
	std::mutex                                                m_pendingResponsesMutex;
	std::unordered_map<jsonrpc::MessageId, ResponseResultPtr> m_pendingResponses;

	std::mutex                                                m_pendingRequestsMutex;
	std::unordered_map<jsonrpc::MessageId, RequestResultPtr>  m_pendingRequests;

	static std::atomic<json::Integer>                         s_uniqueRequestId;

	std::optional<jsonrpc::Response> processRequest(const jsonrpc::Request& request);
	void processResponse(const jsonrpc::Response& response);
	std::optional<jsonrpc::Response> processMessage(const jsonrpc::Message& message);
	jsonrpc::MessageBatch processMessageBatch(const jsonrpc::MessageBatch& batch);
	void addHandler(MessageMethod method, HandlerWrapper&& handlerFunc);
	void addResponseResult(const jsonrpc::MessageId& id, ResponseResultPtr result);
	void sendRequest(MessageMethod method, RequestResultPtr result, const std::optional<json::Any>& params = std::nullopt);
	void sendNotification(MessageMethod method, const std::optional<json::Any>& params = std::nullopt);
	void sendErrorMessage(ErrorCodes code, const std::string& message);

	template<typename T>
	static jsonrpc::Response createResponse(const jsonrpc::MessageId& id, const T& result)
	{
		return jsonrpc::createResponse(id, toJson(result));
	}

	/*
	 * Response result wrapper
	 */

	class ResponseResultBase{
	public:
		virtual ~ResponseResultBase() = default;
		virtual bool isReady() const = 0;
		virtual jsonrpc::Response get() = 0;
	};

	template<typename T>
	class ResponseResult : public ResponseResultBase{
	public:
		ResponseResult(jsonrpc::MessageId id, std::future<T> future)
			: m_id{std::move(id)},
		    m_future{std::move(future)}{}

		bool isReady() const override
		{
			return m_future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
		}

		jsonrpc::Response get() override
		{
			assert(isReady());
			return createResponse(m_id, m_future.get());
		}

	private:
		jsonrpc::MessageId m_id;
		std::future<T>     m_future;
	};

	/*
	 * Request result wrapper
	 */

	class RequestResultBase{
	public:
		virtual ~RequestResultBase() = default;
		virtual void setValueFromJson(const json::Any& json) = 0;
		virtual void setException(std::exception_ptr e) = 0;
	};

	template<typename T>
	class RequestResult : public RequestResultBase{
	public:
		std::future<T> future(){ return m_promise.get_future(); }

		void setValueFromJson(const json::Any& json) override
		{
			T value;
			fromJson(json, value);
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

template<typename MessageType, typename F>
requires IsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, const json::Any& json)
	{
		typename MessageType::Params params;
		fromJson(json, params);

		if constexpr(std::same_as<decltype(f(id, params)), ASyncRequestResult<MessageType>>)
		{
			addResponseResult(id, std::make_unique<ResponseResult<typename MessageType::Result>>(id, f(id, std::move(params))));
			return std::nullopt;
		}
		else
		{
			(void)this;
			return createResponse(id, f(id, std::move(params)));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, const json::Any&)
	{
		if constexpr(std::same_as<decltype(f(id)), ASyncRequestResult<MessageType>>)
		{
			addResponseResult(id, std::make_unique<ResponseResult<typename MessageType::Result>>(f(id)));
			return std::nullopt;
		}
		else
		{
			(void)this;
			return createResponse(id, f(id));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNotificationCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId&, const json::Any& json)
	{
		typename MessageType::Params params;
		fromJson(json, params);
		f(std::move(params));
		return std::nullopt;
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsNotificationCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId&, const json::Any&)
	{
		f();
		return std::nullopt;
	});

	return *this;
}

template<typename MessageType>
requires message::HasParams<MessageType> && message::HasResult<MessageType>
ASyncRequestResult<MessageType> MessageHandler::sendRequest(const typename MessageType::Params& params)
{
	auto result = std::make_unique<RequestResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result), toJson(params));
	return future;
}

template<typename MessageType>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
ASyncRequestResult<MessageType> MessageHandler::sendRequest()
{
	auto result = std::make_unique<RequestResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result));
	return future;
}

} // namespace lsp
