#pragma once

#include "messagehandler.h"

namespace lsp{

/*
 * createResponse
 */

template<typename T>
jsonrpc::Response MessageHandler::createResponse(const MessageId& id, T&& result)
{
	// return jsonrpc::createResponse(id, toJson(std::forward<T>(result)));
	(void)result;
	return jsonrpc::createResponse(id, {});
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
		return jsonrpc::createErrorResponse(id, MessageError::InternalError, e.what());
	}
}

/*
 * add
 */

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&& json, bool allowAsync) -> OptionalResponse
	{
		typename M::Params params;
		fromJson(std::move(json), params);
		const auto& id = currentRequestId();

		if constexpr(IsCallbackResult<AsyncRequestResult<M>, typename M::Params, F>)
		{
			auto future = f(std::move(params));

			if(allowAsync)
			{
				m_threadPool.addTask([this, id = id, future = std::move(future)]() mutable
				{
					auto response = createResponseFromAsyncResult<M>(id, future);
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
			return createResponse(id, f(std::move(params)));
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&&, bool allowAsync) -> OptionalResponse
	{
		const auto& id = currentRequestId();

		if constexpr(IsNoParamsCallbackResult<AsyncRequestResult<M>, F>)
		{
			auto future = f();

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
	[this, f = std::forward<F>(handlerFunc)](json::Value&& json, bool allowAsync) -> OptionalResponse
	{
		typename M::Params params;
		fromJson(std::move(json), params);

		if constexpr(IsCallbackResult<AsyncNotificationResult, typename M::Params, F>)
		{
			auto future = f(std::move(params));

			if(allowAsync)
			{
				m_threadPool.addTask([result = std::move(future)]() mutable
				{
					result.get();
				});
			}
			else
			{
				future.get();
			}
		}
		else
		{
			(void)this;
			(void)allowAsync;
			f(std::move(params));
		}

		return std::nullopt;
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsNotificationCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&&, bool allowAsync) -> OptionalResponse
	{
		if constexpr(IsNoParamsCallbackResult<AsyncNotificationResult, F>)
		{
			auto future = f();

			if(allowAsync)
			{
				m_threadPool.addTask([result = std::move(future)]() mutable
				{
					result.get();
				});
			}
			else
			{
				future.get();
			}
		}
		else
		{
			(void)this;
			(void)allowAsync;
			f();
		}

		return std::nullopt;
	});

	return *this;
}

/*
 * sendRequest
 */

template<typename M, typename F, typename E>
MessageId MessageHandler::sendRequest(const typename M::Params& params, F&& then, E&& error) requires SendRequest<M, F, E>
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = Connection::request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit(m_connection);
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M, typename F, typename E>
MessageId MessageHandler::sendRequest(F&& then, E&& error) requires SendNoParamsRequest<M, F, E>
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = Connection::request(M::Method, requestId);

	requestSender.submit(m_connection);
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest(const typename M::Params& params) requires message::IsRequest<M> && message::HasParams<M>
{
	auto       result         = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future         = result->future();
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = Connection::request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit(m_connection);
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest() requires message::IsRequest<M> && (!message::HasParams<M>)
{
	auto       result         = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future         = result->future();
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = Connection::request(M::Method, requestId);

	requestSender.submit(m_connection);
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

/*
 * sendNotification
 */

template<typename M>
void MessageHandler::sendNotification(const typename M::Params& params) requires SendNotification<M>
{
	auto notificationSender = Connection::notification(M::Method);
	notificationSender.writeParams(params);
	notificationSender.submit(m_connection);
}

template<typename M>
void MessageHandler::sendNotification() requires SendNoParamsNotification<M>
{
	auto notificationSender = Connection::notification(M::Method);
	notificationSender.submit(m_connection);
}

/*
 * FutureRequestResult
 */

template<typename T>
void MessageHandler::FutureRequestResult<T>::setValueFromJson(json::Value&& json)
{
	try
	{
		auto value = T();
		fromJson(std::move(json), value);
		m_promise.set_value(std::move(value));
	}
	catch(const Exception& e)
	{
		m_promise.set_exception(std::make_exception_ptr(e));
	}
}

template<typename T>
void MessageHandler::FutureRequestResult<T>::setError(ResponseError&& error)
{
	m_promise.set_exception(std::make_exception_ptr(std::move(error)));
}

/*
 * CallbackRequestResult
 */

template<typename T, typename F, typename E>
void MessageHandler::CallbackRequestResult<T, F, E>::setValueFromJson(json::Value&& json)
{
	try
	{
		auto value = T();
		fromJson(std::move(json), value);
		m_then(std::move(value));
	}
	catch(const json::Error& error)
	{
		m_error(ResponseError(MessageError::ParseError, error.what()));
	}
}

template<typename T, typename F, typename E>
void MessageHandler::CallbackRequestResult<T, F, E>::setError(ResponseError&& error)
{
	m_error(std::move(error));
}

} // namespace lsp
