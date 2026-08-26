#pragma once

#include "messagehandler.h"

namespace lsp{

/*
 * createResponse
 */

template<typename T>
void MessageHandler::sendResponse(const MessageId& messageId, const T& result)
{
	auto responseSender = m_connection.response(messageId);
	responseSender.writeData(result);
	responseSender.submit();
}

template<typename M>
void MessageHandler::handleAsyncResult(const MessageId* messageId, AsyncRequestResult<M>& result)
{
	try
	{
		const auto value = result.get();

		if(messageId)
			sendResponse(*messageId, value);
	}
	catch(const RequestError& e)
	{
		if(messageId)
			sendErrorResponse(*messageId, e.code(), e.what(), e.data());
	}
	catch(std::exception& e)
	{
		if(messageId)
			sendErrorResponse(*messageId, MessageError::InternalError, e.what());
	}
}

/*
 * add
 */

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&& json, Connection::BatchSender* batchSender)
	{
		typename M::Params params;
		fromJson(std::move(json), params);
		const auto& id = currentRequestId();

		if constexpr(IsCallbackResult<AsyncRequestResult<M>, typename M::Params, F>)
		{
			auto future = f(std::move(params));

			if(batchSender)
			{
				auto responseWriter = batchSender->writeResponse(id);
				responseWriter.writeData(
					[](std::string_view key, const auto& value, json::ObjectWriter& writer)
					{
						toJson(key, value, writer);
					}, future.get());
			}
			else
			{
				m_threadPool.addTask([this, id = id, future = std::move(future)]() mutable
				{
					handleAsyncResult<M>(&id, future);
				});
			}
		}
		else
		{
			(void)this;
			(void)batchSender;
			sendResponse(id, f(std::move(params)));
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsRequestCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&&, Connection::BatchSender* batchSender)
	{
		const auto& id = currentRequestId();

		if constexpr(IsNoParamsCallbackResult<AsyncRequestResult<M>, F>)
		{
			auto future = f();

			if(batchSender)
			{
				auto responseWriter = batchSender->writeResponse(id);
				responseWriter.writeData(
					[](std::string_view key, const auto& value, json::ObjectWriter& writer)
					{
						toJson(key, value, writer);
					}, future.get());
			}
			else
			{
				m_threadPool.addTask([this, id = id, result = std::move(future)]() mutable
				{
					handleAsyncResult<M>(&id, future);
				});
			}
		}
		else
		{
			(void)this;
			(void)batchSender;
			sendResponse(id, f());
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNotificationCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&& json, Connection::BatchSender*)
	{
		typename M::Params params;
		fromJson(std::move(json), params);

		if constexpr(IsCallbackResult<AsyncNotificationResult, typename M::Params, F>)
		{
			auto future = f(std::move(params));

			m_threadPool.addTask([result = std::move(future)]() mutable
			{
				result.get();
			});
		}
		else
		{
			(void)this;
			f(std::move(params));
		}
	});

	return *this;
}

template<typename M, typename F>
MessageHandler& MessageHandler::add(F&& handlerFunc) requires IsNoParamsNotificationCallback<M, F>
{
	addHandler(M::Method,
	[this, f = std::forward<F>(handlerFunc)](json::Value&&, Connection::BatchSender*)
	{
		if constexpr(IsNoParamsCallbackResult<AsyncNotificationResult, F>)
		{
			auto future = f();

			m_threadPool.addTask([result = std::move(future)]() mutable
			{
				result.get();
			});
		}
		else
		{
			(void)this;
			f();
		}
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
	auto       requestSender  = m_connection.request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M, typename F, typename E>
MessageId MessageHandler::sendRequest(F&& then, E&& error) requires SendNoParamsRequest<M, F, E>
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = m_connection.request(M::Method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest(const typename M::Params& params) requires message::IsRequest<M> && message::HasParams<M>
{
	auto       result         = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future         = result->future();
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = m_connection.request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

template<typename M>
FutureResponse<M> MessageHandler::sendRequest() requires message::IsRequest<M> && (!message::HasParams<M>)
{
	auto       result         = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future         = result->future();
	const auto requestId      = nextUniqueRequestId();
	auto       requestSender  = m_connection.request(M::Method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

/*
 * sendNotification
 */

template<typename M>
void MessageHandler::sendNotification(const typename M::Params& params) requires SendNotification<M>
{
	auto notificationSender = m_connection.notification(M::Method);
	notificationSender.writeParams(params);
	notificationSender.submit();
}

template<typename M>
void MessageHandler::sendNotification() requires SendNoParamsNotification<M>
{
	auto notificationSender = m_connection.notification(M::Method);
	notificationSender.submit();
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
