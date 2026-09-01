#pragma once

#include <cassert>
#include <concepts>
#include "message_handler.h"

namespace lsp{

/*
 * sendResponse
 */

template<typename T>
void MessageHandler::sendResponse(const MessageId& messageId, const T& result, Connection::BatchSender* batchSender)
{
	if(batchSender)
	{
		auto responseWriter = batchSender->writeResponse(messageId);
		responseWriter.writeData(
			[](std::string_view key, const T& value, json::ObjectWriter& objectWriter)
			{
				writeJson(key, value, objectWriter);
			}, result);
	}
	else
	{
		auto responseSender = m_connection.response(messageId);
		responseSender.writeData(result);
		responseSender.submit();
	}
}

template<typename T>
void MessageHandler::handleRequestResult(RequestResult<T>& result, Connection::BatchSender* batchSender)
{
	try
	{
		sendResponse(result.requestId(), result.get(), batchSender);
	}
	catch(const RequestError& e)
	{
		sendErrorResponse(result.requestId(), e.code(), e.what(), e.data(), batchSender);
	}
	catch(std::exception& e)
	{
		sendErrorResponse(result.requestId(), MessageError::InternalError, e.what(), {}, batchSender);
	}
}

/*
 * on
 */

template<typename M, typename F>
auto MessageHandler::on(F&& callback) -> MessageHandler&
{
	return onCustom<M>(M::Method, std::forward<F>(callback));
}

template<typename M, typename F>
auto MessageHandler::onCustom(std::string_view method, F&& callback) -> MessageHandler&
{
	addHandler(method,
		[this, callback = std::forward<F>(callback)](
			[[maybe_unused]] json::Value&& json, [[maybe_unused]] const RequestId* requestId, [[maybe_unused]] Connection::BatchSender* batchSender) mutable
		{
			if constexpr(M::Kind == MessageKind::Request)
			{
				assert(requestId);

				auto context = RequestContext(*this, *requestId);

				auto result =
					[&json, &callback, requestId]() mutable
					{
						if constexpr(MessageHasParams<M>)
						{
							static_assert(std::invocable<F, typename M::Params>, "Request callback must be callable with matching params");

							auto params = typename M::Params();
							fromJson(std::move(json), params);
							return RequestResult(callback(std::move(params)), *requestId);
						}
						else
						{
							(void)json;
							static_assert(std::invocable<F>, "Request callback must be callable without params");
							return RequestResult(callback(), *requestId);
						}
					}();

				// Requests that are part of a batch cannot be handled asynchronously
				if(!result.isAsync() || batchSender)
				{
					handleRequestResult(result, batchSender);
				}
				else
				{
					m_threadPool.addTask(
						[this, requestId = *requestId, result = std::move(result)]() mutable
						{
							auto context = RequestContext(*this, std::move(requestId));
							handleRequestResult(result, nullptr);
						});
				}
			}
			else // Notification
			{
				(void)this;
				static_assert(M::Kind == MessageKind::Notification);
				assert(!requestId);

				if constexpr(MessageHasParams<M>)
				{
					static_assert(std::invocable<F, typename M::Params>, "Notification callback must be callable with matching params");

					auto params = typename M::Params();
					fromJson(std::move(json), params);

					if constexpr(IsFuture<std::invoke_result_t<F, typename M::Params>>{})
						m_threadPool.addTask([future = callback(std::move(params))](){ future.wait(); });
					else
						callback(std::move(params));
				}
				else
				{
					static_assert(std::invocable<F>, "Notification callback must be callable without params");

					if constexpr(IsFuture<std::invoke_result_t<F>>{})
						m_threadPool.addTask([future = callback()](){ future.wait(); });
					else
						callback();
				}
			}
		});

	return *this;
}

/*
 * sendRequest
 */

template<typename M, typename F, typename E>
requires MessageHasParams<M>
auto MessageHandler::sendRequest(const typename M::Params& params, F&& then, E&& error) -> MessageId
{
	return sendCustomRequest<M>(M::Method, params, std::forward<F>(then), std::forward<E>(error));
}

template<typename M, typename F, typename E>
requires MessageHasParams<M>
auto MessageHandler::sendCustomRequest(std::string_view method, const typename M::Params& params, F&& then, E&& error) -> MessageId
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, std::decay_t<F>, std::decay_t<E>>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M, typename F, typename E>
requires (!MessageHasParams<M>)
auto MessageHandler::sendRequest(F&& then, E&& error) -> MessageId
{
	return sendCustomRequest<M>(M::Method, std::forward<F>(then), std::forward<E>(error));
}

template<typename M, typename F, typename E>
requires (!MessageHasParams<M>)
auto MessageHandler::sendCustomRequest(std::string_view method, F&& then, E&& error) -> MessageId
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, std::decay_t<F>, std::decay_t<E>>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M>
requires MessageHasParams<M> && MessageHasResult<M>
auto MessageHandler::sendRequest(const typename M::Params& params) -> RequestResult<typename M::Result>
{
	return sendCustomRequest<M>(M::Method, params);
}

template<typename M>
requires MessageHasParams<M> && MessageHasResult<M>
auto MessageHandler::sendCustomRequest(std::string_view method, const typename M::Params& params) -> RequestResult<typename M::Result>
{
	auto       result        = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future        = result->future();
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return RequestResult(std::move(future), requestId);
}

template<typename M>
requires (!MessageHasParams<M>) && MessageHasResult<M>
auto MessageHandler::sendRequest() -> RequestResult<typename M::Result>
{
	return sendCustomRequest<M>(M::Method);
}

template<typename M>
requires (!MessageHasParams<M>) && MessageHasResult<M>
auto MessageHandler::sendCustomRequest(std::string_view method) -> RequestResult<typename M::Result>
{
	auto       result        = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future        = result->future();
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return RequestResult(std::move(future), requestId);
}

/*
 * sendNotification
 */

template<typename M>
requires MessageHasParams<M> && (!MessageHasResult<M>)
void MessageHandler::sendNotification(const typename M::Params& params)
{
	sendCustomNotification<M>(M::Method, params);
}

template<typename M>
requires MessageHasParams<M> && (!MessageHasResult<M>)
void MessageHandler::sendCustomNotification(std::string_view method, const typename M::Params& params)
{
	auto notificationSender = m_connection.notification(method);
	notificationSender.writeParams(params);
	notificationSender.submit();
}

template<typename M>
requires (!MessageHasParams<M>) && (!MessageHasResult<M>)
void MessageHandler::sendNotification()
{
	sendCustomNotification<M>(M::Method);
}

template<typename M>
requires (!MessageHasParams<M>) && (!MessageHasResult<M>)
void MessageHandler::sendCustomNotification(std::string_view method)
{
	auto notificationSender = m_connection.notification(method);
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
	static_assert(std::invocable<F, T>,
		"Response callback must be callable with request result");
	static_assert(std::invocable<E, const ResponseError&>,
		"Response error callback must be callable with const RequestError&");

	try
	{
		auto value = T();
		fromJson(std::move(json), value);
		m_then(std::move(value));
	}
	catch(const json::Error& error)
	{
		m_error(ResponseError(MessageError::InvalidParams, error.what()));
	}
}

template<typename T, typename F, typename E>
void MessageHandler::CallbackRequestResult<T, F, E>::setError(ResponseError&& error)
{
	m_error(std::move(error));
}

} // namespace lsp
