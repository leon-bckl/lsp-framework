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
	using FutureType = std::future<T>;

	RequestResult(RequestId requestId, T&& result)
		: m_requestId{std::move(requestId)}
		, m_result{std::move(result)}
	{
	}

	RequestResult(RequestId requestId, FutureType&& future)
		: m_requestId{std::move(requestId)}
		, m_result{std::move(future)}
	{
	}

	[[nodiscard]] auto requestId() const -> const RequestId&{ return m_requestId; }
	[[nodiscard]] auto isAsync() const -> bool{ return std::holds_alternative<FutureType>(m_result); }

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

	[[nodiscard]] auto get() -> T
	{
		if(auto* future = std::get_if<FutureType>(&m_result))
			return future->get();

		return std::move(std::get<T>(m_result));
	}

private:
	RequestId                   m_requestId;
	std::variant<T, FutureType> m_result;
};

} // namespace lsp
