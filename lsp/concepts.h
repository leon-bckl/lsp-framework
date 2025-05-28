#pragma once

#include <concepts>
#include <lsp/error.h>
#include <lsp/messagebase.h>
#include <lsp/requestresult.h>

namespace lsp{

/*
 * Concepts to verify the type of callback
 */


template<typename T, typename F>
concept IsNoParamsCallbackResult = std::invocable<F> && std::same_as<std::invoke_result_t<F>, T>;

template<typename T, typename P, typename F>
concept IsCallbackResult = std::invocable<F, P> && std::same_as<std::invoke_result_t<F, P>, T>;

template<typename M, typename F>
concept IsRequestCallbackResult = IsCallbackResult<typename M::Result, typename M::Params, F> ||
                                  IsCallbackResult<AsyncRequestResult<M>, typename M::Params, F>;

template<typename M, typename F>
concept IsNoParamsRequestCallbackResult = IsNoParamsCallbackResult<typename M::Result, F> ||
                                          IsNoParamsCallbackResult<AsyncRequestResult<M>, F>;

template<typename M, typename F>
concept IsNotificationCallbackResult = IsCallbackResult<void, typename M::Params, F>;

template<typename M, typename F>
concept IsNoParamsNotificationCallbackResult = IsNoParamsCallbackResult<void, F>;

template<typename M, typename F>
concept IsRequestCallback = message::HasParams<M> &&
                            message::HasResult<M> &&
                            IsRequestCallbackResult<M, F>;

template<typename M, typename F>
concept IsNoParamsRequestCallback = !message::HasParams<M> &&
                                    message::HasResult<M> &&
                                    IsNoParamsRequestCallbackResult<M, F>;

template<typename M, typename F>
concept IsNotificationCallback = message::HasParams<M> &&
                                 !message::HasResult<M> &&
                                 IsNotificationCallbackResult<M, F>;

template<typename M, typename F>
concept IsNoParamsNotificationCallback = !message::HasParams<M> &&
                                         !message::HasResult<M> &&
                                         IsNoParamsNotificationCallbackResult<M, F>;


template<typename M, typename F>
concept IsResponseCallback = std::invocable<F, typename M::Result&&>;

template<typename F>
concept IsResponseErrorCallback = std::invocable<F, const ResponseError&>;

template<typename M, typename F, typename E>
concept SendRequest = message::IsRequest<M> && message::HasParams<M> &&
                      IsResponseCallback<M, F> &&
                      IsResponseErrorCallback<E>;

template<typename M, typename F, typename E>
concept SendNoParamsRequest = message::IsRequest<M> && (!message::HasParams<M>) &&
                              IsResponseCallback<M, F> &&
                              IsResponseErrorCallback<E>;

template<typename M>
concept SendNotification = message::IsNotification<M> && message::HasParams<M>;

template<typename M>
concept SendNoParamsNotification = message::IsNotification<M> && (!message::HasParams<M>);


} // namespace lsp
