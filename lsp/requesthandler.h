#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <lsp/connection.h>
#include <lsp/messagebase.h>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

class ErrorCodes;
class Connection;

template<typename MessageType, typename F>
concept IsRequestCallback = (message::HasParams<MessageType> &&
                             message::HasResult<MessageType> &&
                             (std::convertible_to<F, std::function<typename MessageType::Result(const jsonrpc::MessageId&, typename MessageType::Params&&)>> ||
                              std::convertible_to<F, std::function<AsyncRequestResult<MessageType>(const jsonrpc::MessageId&, typename MessageType::Params&&)>>));

template<typename MessageType, typename F>
concept IsNoParamsRequestCallback = ((!message::HasParams<MessageType>) &&
                                     message::HasResult<MessageType> &&
                                     (std::convertible_to<F, std::function<typename MessageType::Result(const jsonrpc::MessageId&)>> ||
                                      std::convertible_to<F, std::function<AsyncRequestResult<MessageType>(const jsonrpc::MessageId&, typename MessageType::Params&&)>>));

template<typename MessageType, typename F>
concept IsNotificationCallback = (message::HasParams<MessageType> &&
							  						      (!message::HasResult<MessageType>) &&
							  						      std::convertible_to<F, std::function<void(typename MessageType::Params&&)>>);

template<typename MessageType, typename F>
concept IsNoParamsNotificationCallback = ((!message::HasParams<MessageType>) &&
                                          (!message::HasResult<MessageType>) &&
                                          std::convertible_to<F, std::function<void()>>);

/**
 * The RequestHandler class is used to send and receive requests and notifications.
 * A new thread is spawned that waits for the results of async requests and sends the responses.
 *
 * Callbacks for the different message types can be registered using the 'add' method.
 * To avoid copying too much data, request parameters are passed as an rvalue reference.
 * A callback is any callable object that matches the return and parameter types of the message.
 * The first parameter of all request callbacks should be the id 'const jsonrpc::MessageId&'. Notification callbacks don't have an id parameter and don't return a value.
 * Here's a callback for the Initialize request:
 *
 * requestHandler.add<lsp::requests::Initialize>([](const jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
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
 * Make sure not to call std::future::wait on the same thread that reads from the connection since it would block.
 */
class RequestHandler : public RequestHandlerInterface{
public:
	RequestHandler(Connection& connection);
	~RequestHandler();

	/*
	 * Callback registration
	 */

	template<typename MessageType, typename F>
	requires IsRequestCallback<MessageType, F>
	RequestHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNoParamsRequestCallback<MessageType, F>
	RequestHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNotificationCallback<MessageType, F>
	RequestHandler& add(F&& handlerFunc);

	template<typename MessageType, typename F>
	requires IsNoParamsNotificationCallback<MessageType, F>
	RequestHandler& add(F&& handlerFunc);

private:
	class ResponseResultBase;
	using ResponseResultPtr = std::unique_ptr<ResponseResultBase>;
	using OptionalResponse = std::optional<jsonrpc::Response>;
	using HandlerWrapper = std::function<OptionalResponse(const jsonrpc::MessageId&, json::Any&&, bool)>;

	Connection&                                               m_connection;
	std::vector<HandlerWrapper>                               m_requestHandlers;
	std::mutex                                                m_requestHandlersMutex;

	bool                                                      m_running = true;
	std::thread                                               m_asyncResponseThread;
	std::mutex                                                m_pendingResponsesMutex;
	std::unordered_map<jsonrpc::MessageId, ResponseResultPtr> m_pendingResponses;

	void onRequest(jsonrpc::Request&& request) override;
	void onRequestBatch(jsonrpc::RequestBatch&& batch) override;

	OptionalResponse processRequest(jsonrpc::Request&& request, bool allowAsync);
	void addHandler(MessageMethod method, HandlerWrapper&& handlerFunc);
	void addResponseResult(const jsonrpc::MessageId& id, ResponseResultPtr result);
	void sendErrorMessage(ErrorCodes code, const std::string& message);

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
};

template<typename MessageType, typename F>
requires IsRequestCallback<MessageType, F>
RequestHandler& RequestHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, json::Any&& json, bool allowAsync) -> OptionalResponse
	{
		typename MessageType::Params params;
		fromJson(std::move(json), params);

		if constexpr(std::same_as<decltype(f(id, std::move(params))), AsyncRequestResult<MessageType>>)
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
			return createResponse(id, f(id, std::move(params)));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNoParamsRequestCallback<MessageType, F>
RequestHandler& RequestHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [this, f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId& id, json::Any&&, bool allowAsync) -> OptionalResponse
	{
		if constexpr(std::same_as<decltype(f(id)), AsyncRequestResult<MessageType>>)
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
			return createResponse(id, f(id));
		}
	});

	return *this;
}

template<typename MessageType, typename F>
requires IsNotificationCallback<MessageType, F>
RequestHandler& RequestHandler::add(F&& handlerFunc)
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
RequestHandler& RequestHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method, [f = std::forward<F>(handlerFunc)](const jsonrpc::MessageId&, json::Any&&, bool) -> OptionalResponse
	{
		f();
		return std::nullopt;
	});

	return *this;
}


} // namespace lsp
