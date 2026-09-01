#pragma once

#include <chrono>
#include <future>
#include <variant>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

using RequestId = jsonrpc::MessageId;

template<typename T>
class RequestResult{
public:
	using ValueType  = T;
	using FutureType = std::future<ValueType>;

	RequestResult(ValueType&& value, RequestId requestId = {})
		: m_result{std::move(value)}
		, m_requestId{std::move(requestId)}
	{
	}

	RequestResult(FutureType&& future, RequestId requestId = {})
		: m_result{std::move(future)}
		, m_requestId{std::move(requestId)}
	{
	}

	[[nodiscard]] auto requestId() const -> const RequestId&{ return m_requestId; }
	[[nodiscard]] auto isAsync() const -> bool{ return std::holds_alternative<FutureType>(m_result); }
	[[nodiscard]] auto isReady() const -> bool{ return wait(0); }

	[[nodiscard]] auto wait(int timeoutMs = -1) const -> bool
	{
		if(auto* future = std::get_if<FutureType>(&m_result))
		{
			if(timeoutMs >= 0)
				return future->wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready;

			future->wait();
		}

		return true;
	}

	[[nodiscard]] auto get() -> ValueType
	{
		if(auto* future = std::get_if<FutureType>(&m_result))
			return future->get();

		return std::move(std::get<ValueType>(m_result));
	}

private:
	std::variant<ValueType, FutureType> m_result;
	RequestId                           m_requestId;

	friend class MessageHandler;
	RequestResult(RequestResult&& other, RequestId requestId)
		: RequestResult{std::move(other)}
	{
		m_requestId = std::move(requestId);
	}
};

} // namespace lsp
