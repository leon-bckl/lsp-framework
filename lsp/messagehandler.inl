#pragma once

#include "messagehandler.h"

namespace lsp{

/*
 * RequestHandler::ResponseResult
 */

template<typename T>
MessageHandler::ResponseResult<T>::ResponseResult(MessageId id, std::future<T> future)
	: m_id{std::move(id)}
	, m_future{std::move(future)}
{
	if(!m_future.valid())
		throw RequestError{jsonrpc::Error::InternalError, "Request handler returned invalid result"};
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
		return jsonrpc::createErrorResponse(m_id, jsonrpc::Error::InternalError, e.what());
	}
}

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
	addHandler(MessageType::Method,
	[this, f = std::forward<F>(handlerFunc)](const MessageId& id, json::Any&&, bool allowAsync) -> OptionalResponse
	{
		if constexpr(std::same_as<
			             std::invoke_result_t<F, MessageId>,
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
