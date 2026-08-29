#pragma once

#include "messagehandler.h"

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

template<typename M>
void MessageHandler::handleRequestResult(const MessageId* messageId, RequestResult<M>& result, Connection::BatchSender* batchSender)
{
	try
	{
		if(messageId)
			sendResponse(*messageId, result.get(), batchSender);
		else
			(void)result.get();
	}
	catch(const RequestError& e)
	{
		if(messageId)
			sendErrorResponse(*messageId, e.code(), e.what(), e.data(), batchSender);
	}
	catch(std::exception& e)
	{
		if(messageId)
			sendErrorResponse(*messageId, MessageError::InternalError, e.what(), {}, batchSender);
	}
}

/*
 * on
 */

template<typename M, typename F>
requires IsRequestCallback<M, F> || IsNoParamsRequestCallback<M, F>
auto MessageHandler::on(F&& callback) -> MessageHandler&
{
	addHandler(M::Method,
		[this, callback = std::forward<F>(callback)](json::Value&& json, Connection::BatchSender* batchSender) mutable
		{
			const auto& requestId = currentRequestId();

			auto result =
				[&json, &callback]() mutable
				{
					if constexpr(requires{typename M::Params;})
					{
						auto params = typename M::Params();
						fromJson(std::move(json), params);
						return RequestResult<M>(callback(std::move(params)));
					}
					else
					{
						(void)json;
						return RequestResult<M>(callback());
					}
				}();

			if(result.isAsync() && !batchSender)
			{
				m_threadPool.addTask(
					[this, requestId = requestId, result = std::move(result)]() mutable
					{
						handleRequestResult<M>(&requestId, result, nullptr);
					});
			}
			else
			{
				handleRequestResult<M>(&requestId, result, batchSender);
			}
		});

	return *this;
}

template<typename M, typename F>
requires IsNotificationCallback<M, F> || IsNoParamsNotificationCallback<M, F>
auto MessageHandler::on(F&& callback) -> MessageHandler&
{
	addHandler(M::Method,
		[callback = std::forward<F>(callback)](json::Value&& json, Connection::BatchSender*)
		{
			if constexpr(requires{typename M::Params;})
			{
				auto params = typename M::Params();
				fromJson(std::move(json), params);
				callback(std::move(params));
			}
			else
			{
				callback();
			}
		});

	return *this;
}

/*
 * sendRequest
 */

template<typename M, typename F, typename E>
requires SendRequest<M, F, E>
auto MessageHandler::sendRequest(const typename M::Params& params, F&& then, E&& error) -> MessageId
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M, typename F, typename E>
requires SendNoParamsRequest<M, F, E>
auto MessageHandler::sendRequest(F&& then, E&& error) -> MessageId
{
	auto result = std::make_unique<CallbackRequestResult<typename M::Result, F, E>>(
		std::forward<F>(then), std::forward<E>(error));
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(M::Method, requestId);

	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return requestId;
}

template<typename M>
requires message::IsRequest<M> && message::HasParams<M>
auto MessageHandler::sendRequest(const typename M::Params& params) -> FutureResponse<M>
{
	auto       result        = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future        = result->future();
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(M::Method, requestId);

	requestSender.writeParams(params);
	requestSender.submit();
	addPendingRequest(std::move(result), requestId);

	return {requestId, std::move(future)};
}

template<typename M>
requires message::IsRequest<M> && (!message::HasParams<M>)
auto MessageHandler::sendRequest() -> FutureResponse<M>
{
	auto       result        = std::make_unique<FutureRequestResult<typename M::Result>>();
	auto       future        = result->future();
	const auto requestId     = nextUniqueRequestId();
	auto       requestSender = m_connection.request(M::Method, requestId);

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
