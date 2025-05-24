#pragma once

#include "messagehandler.h"

namespace lsp{

/*
 * RequestHandler::add
 */

template<typename MessageType, typename F>
requires IsRequestCallback<MessageType, F>
MessageHandler& MessageHandler::add(F&& handlerFunc)
{
	addHandler(MessageType::Method,
	[this, f = std::forward<F>(handlerFunc)](const MessageId& id, json::Any&& json, bool allowAsync) -> OptionalResponse
	{
		typename MessageType::Params params;
		fromJson(std::move(json), params);

		if constexpr(std::same_as<
			             std::invoke_result_t<F, MessageId, typename MessageType::Params>,
			             AsyncRequestResult<MessageType>
		             >)
		{
			auto future = f(id, std::move(params));

			if(allowAsync)
			{
				m_threadPool.addTask([this, id = id](AsyncRequestResult<MessageType>&& result) mutable
				{
					auto response = createResponseFromAsyncResult<MessageType>(std::move(id), result);
					sendResponse(std::move(response));
				}, std::move(future));

				return std::nullopt;
			}

			return createResponse(id, future.get());
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
	addHandler(MessageType::Method,
	[this, f = std::forward<F>(handlerFunc)](const MessageId& id, json::Any&&, bool allowAsync) -> OptionalResponse
	{
		if constexpr(std::same_as<
			             std::invoke_result_t<F, MessageId>,
			             AsyncRequestResult<MessageType>
		             >)
		{
			auto future = f(id);

			if(allowAsync)
			{
				m_threadPool.addTask([this, id = id, result = std::move(future)]() mutable
				{
					auto response = createResponseFromAsyncResult<MessageType>(std::move(id), result);
					sendResponse(std::move(response));
				});

				return std::nullopt;
			}

			return createResponse(id, future.get());
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
	addHandler(MessageType::Method,
	[f = std::forward<F>(handlerFunc)](const MessageId&, json::Any&& json, bool) -> OptionalResponse
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
	addHandler(MessageType::Method,
	[f = std::forward<F>(handlerFunc)](const MessageId&, json::Any&&, bool) -> OptionalResponse
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
MessageId MessageHandler::sendRequest(typename MessageType::Params&& params, F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result), toJson(std::move(params)));
}

template<typename MessageType, typename F, typename E>
requires message::HasResult<MessageType> && (!message::HasParams<MessageType>) &&
         std::invocable<F, typename MessageType::Result&&> &&
         std::invocable<E, const Error&>
MessageId MessageHandler::sendRequest(F&& then, E&& error)
{
	auto result = std::make_unique<CallbackRequestResult<typename MessageType::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(MessageType::Method, std::move(result));
}

} // namespace lsp
