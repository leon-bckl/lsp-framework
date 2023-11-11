#pragma once

#include <vector>
#include <utility>
#include <concepts>
#include <functional>
#include "messagebase.h"

namespace lsp{

template<typename T>
concept HasParams = requires(T t){
	{t.params};
};

template<typename T>
concept HasResult = requires(T t){
	{t.result};
};

class MessageHandler{
public:
	template<typename MessageType, typename F>
	requires std::derived_from<MessageType, Message>
	MessageHandler& request(F&& handlerFunc){
		constexpr auto index = static_cast<std::size_t>(MessageType::MessageMethod);

		if(m_requestHandlers.size() <= index)
			m_requestHandlers.resize(index + 1);

		m_requestHandlers[index] = [handlerFunc](Message& message){
			handleRequest(dynamic_cast<MessageType&>(message), handlerFunc);
		};

		return *this;
	}

	template<typename MessageType, typename F>
	requires std::derived_from<MessageType, Message> && HasResult<MessageType>
	MessageHandler& response(F&& handlerFunc){
		constexpr auto index = static_cast<std::size_t>(MessageType::MessageMethod);

		if(m_responseHandlers.size() <= index)
			m_responseHandlers.resize(index + 1);

		m_responseHandlers[index] = [handlerFunc](Message& message){
			handlerFunc(std::as_const(dynamic_cast<MessageType&>(message).result));
		};

		return *this;
	}

	void processRequest(Message& message){
		if(handlesRequest(message.method()))
			m_requestHandlers[static_cast<std::size_t>(message.method())](message);
	}

	void processResponse(Message& message){
		if(handlesResponse(message.method()))
			m_responseHandlers[static_cast<std::size_t>(message.method())](message);
	}

	bool handlesRequest(messages::Method method){
		auto index = static_cast<std::size_t>(method);
		return index < m_requestHandlers.size() && m_requestHandlers[index];
	}

	bool handlesResponse(messages::Method method){
		auto index = static_cast<std::size_t>(method);
		return index < m_responseHandlers.size() && m_requestHandlers[index];
	}

private:
	using HandlerWrapper = std::function<void(Message&)>;
	std::vector<HandlerWrapper> m_requestHandlers;
	std::vector<HandlerWrapper> m_responseHandlers;

	template<typename MessageType, typename F>
	requires HasParams<MessageType> && (!HasResult<MessageType>)
	static void handleRequest(MessageType& message, F&& handler){
		handler(std::as_const(message.params));
	}

	template<typename MessageType, typename F>
	requires HasResult<MessageType> && (!HasParams<MessageType>)
	static void handleRequest(MessageType& message, F&& handler){
		handler(message.result);
	}

	template<typename MessageType, typename F>
	requires HasParams<MessageType> && HasResult<MessageType>
	static void handleRequest(MessageType& message, F&& handler){
		handler(std::as_const(message.params), message.result);
	}

	template<typename MessageType, typename F>
	requires (!HasParams<MessageType>) && (!HasResult<MessageType>)
	static void handleRequest(MessageType&, F&& handler){
		handler();
	}
};

} // namespace lsp
