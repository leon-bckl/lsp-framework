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

template<typename MessageType, typename F>
requires HasParams<MessageType> && (!HasResult<MessageType>)
void handleMessage(MessageType& message, F&& handler){
	handler(std::as_const(message.params));
}

template<typename MessageType, typename F>
requires HasResult<MessageType> && (!HasParams<MessageType>)
void handleMessage(MessageType& message, F&& handler){
	handler(message.result);
}

template<typename MessageType, typename F>
requires HasParams<MessageType> && HasResult<MessageType>
void handleMessage(MessageType& message, F&& handler){
	handler(std::as_const(message.params), message.result);
}

template<typename MessageType, typename F>
requires (!HasParams<MessageType>) && (!HasResult<MessageType>)
void handleMessage(MessageType&, F&& handler){
	handler();
}

class MessageHandler{
public:
	template<typename MessageType, typename F>
	requires std::derived_from<MessageType, Message>
	MessageHandler& add(F&& handlerFunc){
		constexpr auto index = static_cast<std::size_t>(MessageType::MessageMethod);

		if(m_handlers.size() <= index)
			m_handlers.resize(index + 1);

		m_handlers[index] = [handlerFunc](Message& message){
			handleMessage(dynamic_cast<MessageType&>(message), handlerFunc);
		};

		return *this;
	}

	void processMessage(Message& message){
		if(handlesMessage(message.method()))
			m_handlers[static_cast<std::size_t>(message.method())](message);
	}

	bool handlesMessage(messages::Method method){
		auto index = static_cast<std::size_t>(method);
		return index < m_handlers.size() && m_handlers[index];
	}

private:
	using HandlerWrapper = std::function<void(Message&)>;
	std::vector<HandlerWrapper> m_handlers;
};

} // namespace lsp
