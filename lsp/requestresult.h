#pragma once

#include <chrono>
#include <future>
#include <variant>
#include <lsp/jsonrpc/jsonrpc.h>

namespace lsp{

using MessageId = jsonrpc::MessageId;

template<typename MessageType>
class RequestResult{
public:
	using ResultType = typename MessageType::Result;
	using FutureType = std::future<ResultType>;

	RequestResult(MessageId requestId, typename MessageType::Result&& result)
		: m_requestId{std::move(requestId)}
		, m_result{std::move(result)}
	{
	}

	RequestResult(MessageId requestId, FutureType&& future)
		: m_requestId{std::move(requestId)}
		, m_result{std::move(future)}
	{
	}

	[[nodiscard]] auto requestId() const -> const MessageId&{ return m_requestId; }
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

	[[nodiscard]] auto get() -> ResultType
	{
		if(auto* future = std::get_if<FutureType>(&m_result))
			return future->get();

		return std::move(std::get<ResultType>(m_result));
	}

private:
	MessageId                            m_requestId;
	std::variant<ResultType, FutureType> m_result;
};

} // namespace lsp
