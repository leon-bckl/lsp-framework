#pragma once

#include <mutex>
#include <future>
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

template<typename MessageType, typename F>
concept IsRequestCallback = (message::HasParams<MessageType> &&
                             message::HasResult<MessageType> &&
                             std::convertible_to<F, std::function<typename MessageType::Result(typename MessageType::Params&&)>>);

template<typename MessageType, typename F>
concept IsNoParamsRequestCallback = ((!message::HasParams<MessageType>) &&
                                     message::HasResult<MessageType> &&
                                     std::convertible_to<F, std::function<typename MessageType::Result()>>);

template<typename MessageType, typename F>
concept IsNotificationCallback = (message::HasParams<MessageType> &&
							  						      (!message::HasResult<MessageType>) &&
							  						      std::convertible_to<F, std::function<void(typename MessageType::Params&&)>>);

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallback = ((!message::HasParams<MessageType>) &&
                                          (!message::HasResult<MessageType>) &&
                                          std::convertible_to<F, std::function<void()>>);

/*
 * The MessageHandler is used to send or receive requests and notifications.
 * 'processIncomingMessages' will read all messages from the connection and invoke callbacks or update the result of sent requests.
 *
 * Callbacks for the different message types can be registered using the 'add' method.
 * A callback can by any callable type that has the same return and parameter types as the message itself.
 * For example the following registers a callback for the Initialize request:
 *
 * messageHandler->add<requests::Initialize>([](const auto& params)
 * {
 *     return InitializeResult{};
 * });
 *
 * There are also convenience typedefs for the message return and parameter types:
 *  - MessageType::Result
 *  - MessageType::Params
 *
 * If an error occurs the callback should throw a RequestError with a fitting error code and description.
 *
 * The 'sendRequest' method returns a std::future for the result type.
 * Make sure not to call std::future::wait on the same thread that calls 'processIncomingMessages' as it would block.
 */
class MessageHandler{
public:
	MessageHandler(Connection& connection) : m_connection{connection}{}

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
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest(const typename MessageType::Params& params);

	template<typename MessageType>
	requires message::HasParams<MessageType> && message::HasResult<MessageType> && message::HasPartialResult<MessageType>
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest(const typename MessageType::Params& params);

	template<typename MessageType>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest();

	template<typename MessageType>
	requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) && (!message::HasPartialResult<MessageType>)
	[[nodiscard]] std::future<typename MessageType::Result> sendRequest();

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
	class ResponseResultBase;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using HandlerWrapper = std::function<std::optional<jsonrpc::Response>(const jsonrpc::MessageId&, const json::Any&)>;

	Connection&                                               m_connection;
	std::vector<HandlerWrapper>                               m_requestHandlers;
	std::mutex                                                m_requestMutex;
	std::unordered_map<jsonrpc::MessageId, ResponseResultPtr> m_pendingRequests;
	json::Integer                                             m_uniqueRequestId = 0;

	std::optional<jsonrpc::Response> processRequest(const jsonrpc::Request& request);
	void processResponse(const jsonrpc::Response& response);
	std::optional<jsonrpc::Response> processMessage(const jsonrpc::Message& message);
	jsonrpc::MessageBatch processMessageBatch(const jsonrpc::MessageBatch& batch);
	void addHandler(MessageMethod method, HandlerWrapper&& handlerFunc);
	void sendRequest(MessageMethod method, ResponseResultPtr result, const std::optional<json::Any>& params = std::nullopt);
	void sendNotification(MessageMethod method, const std::optional<json::Any>& params = std::nullopt);
	void sendErrorMessage(ErrorCodes code, const std::string& message);

	template<typename T>
	jsonrpc::Response createResponse(const jsonrpc::MessageId& id, const T& result)
	{
		return jsonrpc::createResponse(id, toJson(result));
	}

	/*
	 * Response result wrapper
	 */

	class ResponseResultBase{
	public:
		virtual ~ResponseResultBase() = default;
		virtual void setValueFromJson(const json::Any& json) = 0;
		virtual void setException(std::exception_ptr e) = 0;
	};

	template<typename T>
	class ResponseResult : public ResponseResultBase{
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
		return createResponse(id, f(std::move(params)));
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, const json::Any&)
	{
		return createResponse(id, f());
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNotificationCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [handlerFunc](const jsonrpc::MessageId&, const json::Any& json)
	{
		typename MessageType::Params params;
		fromJson(json, params);
		handlerFunc(std::move(params));
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
std::future<typename MessageType::Result> MessageHandler::sendRequest(const typename MessageType::Params& params)
{
	auto result = std::make_unique<ResponseResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result), toJson(params));
	return future;
}

template<typename MessageType>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>)
std::future<typename MessageType::Result> MessageHandler::sendRequest()
{
	auto result = std::make_unique<ResponseResult<typename MessageType::Result>>();
	auto future = result->future();
	sendRequest(MessageType::Method, std::move(result));
	return future;
}

} // namespace lsp
