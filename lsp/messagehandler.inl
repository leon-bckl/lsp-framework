#pragma once

#include "messagehandler.h"

namespace lsp{

/*
 * createResponse
 */

template<typename T>
jsonrpc::Response MessageHandler::createResponse(const MessageId& id, T&& result)
{
	return jsonrpc::createResponse(id, toJson(std::forward<T>(result)));
}

template<typename M>
jsonrpc::Response MessageHandler::createResponseFromAsyncResult(const MessageId& id, AsyncRequestResult<M>& result)
{
	try
	{
		return createResponse(id, result.get());
	}
	catch(const RequestError& e)
	{
		return jsonrpc::createErrorResponse(id, e.code(), e.what());
	}
	catch(std::exception& e)
	{
		return jsonrpc::createErrorResponse(id, Error::InternalError, e.what());
	}
}

/*
 * add
 */

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Any&& json, bool allowAsync) -> OptionalResponse
	{
		typename M::Params params;
		fromJson(std::move(json), params);
		const auto& id = currentRequestId();

		if constexpr(IsCallbackResult<AsyncRequestResult<M>, typename M::Params, F>)
		{
			auto future = f(std::move(params));

			if(allowAsync)
			{
				m_threadPool.addTask([this, id = id](AsyncRequestResult<M>&& result) mutable
				{
					auto response = createResponseFromAsyncResult<M>(id, result);
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
			return createResponse(id, f(std::move(params)));
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Any&&, bool allowAsync) -> OptionalResponse
	{
		const auto& id = currentRequestId();

		if constexpr(IsNoParamsCallbackResult<AsyncRequestResult<M>, F>)
		{
			auto future = f(id);

			if(allowAsync)
			{
				m_threadPool.addTask([this, id = id, result = std::move(future)]() mutable
				{
					auto response = createResponseFromAsyncResult<M>(id, result);
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
			return createResponse(id, f());
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNotificationCallback<M, F>
{
	addHandler(M::Method,
	[f = std::forward<F>(handlerFunc)](json::Any&& json, bool) -> OptionalResponse
	{
		typename M::Params params;
		fromJson(std::move(json), params);
		f(std::move(params));
		return std::nullopt;
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsNotificationCallback<M, F>
{
	addHandler(M::Method,
	[f = std::forward<F>(handlerFunc)](json::Any&&, bool) -> OptionalResponse
	{
		f();
		return std::nullopt;
	});

	return *this;
}

/*
 * sendRequest
 */

template<typename M, typename F, typename E>
MessageId MessageHandler::sendRequest(typename M::Params&& params, F&& then, E&& error) requires SendRequest<M, F, E>
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(M::Method, std::move(result), toJson(std::move(params)));
}

template<typename M, typename F, typename E>
MessageId MessageHandler::sendRequest(F&& then, E&& error) requires SendNoParamsRequest<M, F, E>
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(std::forward<F>(then), std::forward<E>(error));
	return sendRequest(M::Method, std::move(result));
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest(typename M::Params&& params) requires message::IsRequest<M> && message::HasParams<M>
{
	auto result    = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto future    = result->future();
	auto messageId = sendRequest(M::Method, std::move(result), toJson(std::move(params)));
	return {std::move(messageId), std::move(future)};
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest() requires message::IsRequest<M> && (!message::HasParams<M>)
{
	auto result    = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto future    = result->future();
	auto messageId = sendRequest(M::Method, std::move(result));
	return {std::move(messageId), std::move(future)};
}

/*
 * sendNotification
 */

template<typename M>
void MessageHandler::sendNotification(typename M::Params&& params) requires SendNotification<M>
{
	sendNotification(M::Method, toJson(std::move(params)));
}

template<typename M>
void MessageHandler::sendNotification() requires SendNoParamsNotification<M>
{
	sendNotification(M::Method);
}

/*
 * FutureRequestResult
 */

template<typename T>
void MessageHandler::FutureRequestResult<T>::setValueFromJson(json::Any&& json)
{
	T value{};
	fromJson(std::move(json), value);
	m_promise.set_value(std::move(value));
}

template<typename T>
void MessageHandler::FutureRequestResult<T>::setException(std::exception_ptr e)
{
	m_promise.set_exception(e);
}

/*
 * CallbackRequestResult
 */

template<typename T, typename F, typename E>
void MessageHandler::CallbackRequestResult<T, F, E>::setValueFromJson(json::Any&& json)
{
	T value{};
	fromJson(std::move(json), value);
	m_then(std::move(value));
}

template<typename T, typename F, typename E>
void MessageHandler::CallbackRequestResult<T, F, E>::setException(std::exception_ptr e)
{
	try
	{
		std::rethrow_exception(e);
	}
	catch(ResponseError& error)
	{
		m_error(error);
	}
}

} // namespace lsp
