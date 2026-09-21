#pragma once

#include <cassert>
#include <concepts>
#include <utility>
#include "message_handler.h"

namespace lsp{

template<typename T>
void MessageHandler::dispatchMessageLog(MessageLog& msgLog, const T& payload)
{
	if(m_msgLogLevel.load() >= MessageLogLevel::InfoAndPayload)
	{
		auto writer = json::Writer("    ");
		writeJson(payload, writer);
		msgLog.payload = std::move(writer).text();
	}

	dispatchMessageLog(msgLog);
}

/*
 * sendResponse
 */

template<typename M>
void MessageHandler::sendResponse(RequestResult<typename M::Result>& result, Connection::BatchSender* batchSender)
{
	const auto ctx = RequestContext::get();

	// Result.get() can throw if the result invokes a callback or calls std::future::get
	try
	{
		const auto resultValue = result.get();

		if(shouldLog())
		{
			auto msgLog = MessageLog{
				.incoming        = false,
				.method          = ctx.method(),
				.requestDuration = std::chrono::steady_clock::now() - ctx.timestamp(),
				.id              = ctx.id(),
			};

			dispatchMessageLog(msgLog, resultValue);
		}

		if(batchSender)
		{
			auto responseWriter = batchSender->writeResponse(ctx.id());
			responseWriter.writeData(
				[](std::string_view key, const typename M::Result& value, json::ObjectWriter& objectWriter)
				{
					writeJson(key, value, objectWriter);
				}, resultValue);
		}
		else
		{
			auto responseSender = m_connection.response(result.requestId());
			responseSender.writeData(resultValue);
			responseSender.submit();
		}
	}
	catch(const RequestError& e)
	{
		sendErrorResponse(ctx.method(), ctx.timestamp(), ctx.id(), e.code(), e.what(), e.data(), batchSender);
	}
	catch(std::exception& e)
	{
		sendErrorResponse(ctx.method(), ctx.timestamp(), ctx.id(), MessageError::InternalError, e.what(), {}, batchSender);
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
requires (M::Kind == MessageKind::Request)
auto MessageHandler::onCustom(std::string_view method, F&& callback) -> MessageHandler&
{
	addHandler(method,
		[this, method = std::string(method), callback = std::forward<F>(callback)](
			[[maybe_unused]] json::Value&& json, [[maybe_unused]] Connection::BatchSender* batchSender) mutable
		{
			const auto context = RequestContext::get();

			auto result =
				[&json, &callback, &context]() mutable
				{
					if constexpr(MessageHasParams<M>)
					{
						static_assert(std::invocable<F, typename M::Params>,
							"Request callback must be callable with matching params");
						static_assert(std::constructible_from<RequestResult<typename M::Result>,
							std::invoke_result_t<F, typename M::Params>>,
							"Request callback must return a value or callable that can construct a MessageType::Result");

						auto params = typename M::Params();

						try
						{
							fromJson(std::move(json), params);
						}
						catch(const json::Error& e)
						{
							throw RequestError(MessageError::InvalidParams, e.what());
						}

						return RequestResult<typename M::Result>(callback(std::move(params)), context.id());
					}
					else
					{
						(void)json;
						static_assert(std::invocable<F>, "Request callback must be callable without params");
						static_assert(std::constructible_from<RequestResult<typename M::Result>, std::invoke_result_t<F>>,
							"Request callback must return a value or callable that can construct a MessageType::Result");
						return RequestResult<typename M::Result>(callback(), context.id());
					}
				}();

			// Requests that are part of a batch cannot be handled asynchronously
			if(!result.isDeferred() || batchSender)
			{
				sendResponse<M>(result, batchSender);
			}
			else
			{
				m_threadPool.addTask(
					[this, method = method, timestamp = context.timestamp(), requestId = context.id(), result = std::move(result)]() mutable
					{
						auto context = RequestContext(*this, method, requestId, timestamp);
						sendResponse<M>(result, nullptr);
					});
			}
		});

	return *this;
}

template<typename M, typename F>
requires (M::Kind == MessageKind::Notification)
auto MessageHandler::onCustom(std::string_view method, F&& callback) -> MessageHandler&
{
	addHandler(method,
		[this, callback = std::forward<F>(callback)](
			[[maybe_unused]] json::Value&& json, [[maybe_unused]] Connection::BatchSender* batchSender) mutable
		{
			(void)this; // Only used for async requests within if constexpr
			static_assert(M::Kind == MessageKind::Notification);

			if constexpr(MessageHasParams<M>)
			{
				static_assert(std::invocable<F, typename M::Params>, "Notification callback must be callable with matching params");

				auto params = typename M::Params();

				try
				{
					fromJson(std::move(json), params);
				}
				catch(const json::Error&)
				{
					// Swallow invalid params for notifications since no error response is sent
					// Might add an error hook for such cases later...
					return;
				}

				if constexpr(IsFuture<std::invoke_result_t<F, typename M::Params>>{})
					m_threadPool.addTask([future = callback(std::move(params))](){ future.wait(); });
				else if constexpr(std::invocable<std::invoke_result_t<F, typename M::Params>>)
					m_threadPool.addTask(callback(std::move(params)));
				else
					callback(std::move(params));
			}
			else
			{
				static_assert(std::invocable<F>, "Notification callback must be callable without params");

				if constexpr(IsFuture<std::invoke_result_t<F>>{})
					m_threadPool.addTask([future = callback()](){ future.wait(); });
				else if constexpr(std::invocable<std::invoke_result_t<F>>)
					m_threadPool.addTask(callback());
				else
					callback();
			}
		});

	return *this;
}

/*
 * sendRequest
 */

template<typename M, typename F, typename E>
requires MessageHasParams<M>
auto MessageHandler::sendRequest(const typename M::Params& params, F&& then, E&& error) -> RequestId
{
	return sendCustomRequest<M>(M::Method, params, std::forward<F>(then), std::forward<E>(error));
}

template<typename M, typename F, typename E>
requires MessageHasParams<M>
auto MessageHandler::sendCustomRequest(std::string_view method, const typename M::Params& params, F&& then, E&& error) -> RequestId
{
	const auto requestId = nextUniqueRequestId();
	const auto timestamp = std::chrono::steady_clock::now();

	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
			.id       = requestId,
		};

		dispatchMessageLog(msgLog, params);
	}

	auto result = std::make_unique<PendingRequestCallback<typename M::Result, std::decay_t<F>, std::decay_t<E>>>(
		std::string(method),
		timestamp,
		requestId,
		std::forward<F>(then),
		std::forward<E>(error));
	auto requestSender = m_connection.request(method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result));

	return requestId;
}

template<typename M, typename F, typename E>
requires (!MessageHasParams<M>)
auto MessageHandler::sendRequest(F&& then, E&& error) -> RequestId
{
	return sendCustomRequest<M>(M::Method, std::forward<F>(then), std::forward<E>(error));
}

template<typename M, typename F, typename E>
requires (!MessageHasParams<M>)
auto MessageHandler::sendCustomRequest(std::string_view method, F&& then, E&& error) -> RequestId
{
	const auto requestId = nextUniqueRequestId();
	const auto timestamp = std::chrono::steady_clock::now();

	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
			.id       = requestId,
		};

		dispatchMessageLog(msgLog);
	}

	auto result = std::make_unique<PendingRequestCallback<typename M::Result, std::decay_t<F>, std::decay_t<E>>>(
		std::string(method),
		timestamp,
		requestId,
		std::forward<F>(then),
		std::forward<E>(error));
	auto requestSender = m_connection.request(method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result));

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
	const auto requestId = nextUniqueRequestId();
	const auto timestamp = std::chrono::steady_clock::now();

	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
			.id       = requestId,
		};

		dispatchMessageLog(msgLog, params);
	}

	auto       result        = std::make_unique<PendingRequestFuture<typename M::Result>>(std::string(method), timestamp, requestId);
	auto       future        = result->future();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result));

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
	const auto requestId = nextUniqueRequestId();
	const auto timestamp = std::chrono::steady_clock::now();

	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
			.id       = requestId,
		};

		dispatchMessageLog(msgLog);
	}

	auto       result        = std::make_unique<PendingRequestFuture<typename M::Result>>(std::string(method), timestamp, requestId);
	auto       future        = result->future();
	auto       requestSender = m_connection.request(method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result));

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
	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
		};

		dispatchMessageLog(msgLog, params);
	}

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
	if(shouldLog())
	{
		auto msgLog = MessageLog{
			.incoming = false,
			.method   = method,
		};

		dispatchMessageLog(msgLog);
	}

	auto notificationSender = m_connection.notification(method);
	notificationSender.submit();
}

/*
 * RequestResultBase
 */

template<typename T>
auto MessageHandler::PendingRequestBase::setValueFromJson(T& value, json::Value&& json) -> bool
{
	try
	{
		fromJson(std::move(json), value);
	}
	catch(const json::Error& e)
	{
		// If an invalid response was received, report it as an internal error
		setError(ResponseError(MessageError::InternalError, e.what()));
		return false;
	}

	return true;
}

/*
 * PendingRequestCallback
 */

template<typename T, typename F, typename E>
MessageHandler::PendingRequestCallback<T, F, E>::PendingRequestCallback(
	std::string method,
	RequestTimestamp timestamp,
	RequestId id,
	F&& then,
	E&& error)
	: PendingRequestBase(std::move(method), timestamp, std::move(id))
	, m_then(std::forward<F>(then))
	, m_error(std::forward<E>(error))
{
	static_assert(std::invocable<F, T>,
		"Response callback must be callable with request result");
	static_assert(std::invocable<E, const ResponseError&>,
		"Response error callback must be callable with const RequestError&");
}

template<typename T, typename F, typename E>
void MessageHandler::PendingRequestCallback<T, F, E>::setValue(json::Value&& json)
{
	auto value = T();
	if(setValueFromJson(value, std::move(json)))
		m_then(std::move(value));
}

template<typename T, typename F, typename E>
void MessageHandler::PendingRequestCallback<T, F, E>::setError(ResponseError&& error)
{
	m_error(std::move(error));
}

/*
 * PendingRequestFuture
 */

template<typename T>
MessageHandler::PendingRequestFuture<T>::PendingRequestFuture(
	std::string method,
	RequestTimestamp timestamp,
	RequestId id)
	: PendingRequestBase(std::move(method), timestamp, std::move(id))
{
}

template<typename T>
void MessageHandler::PendingRequestFuture<T>::setValue(json::Value&& json)
{
	auto value = T();
	if(setValueFromJson(value, std::move(json)))
		m_promise.set_value(std::move(value));
}

template<typename T>
void MessageHandler::PendingRequestFuture<T>::setError(ResponseError&& error)
{
	m_promise.set_exception(std::make_exception_ptr(std::move(error)));
}

} // namespace lsp
