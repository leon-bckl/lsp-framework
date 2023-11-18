#pragma once

#include <memory>
#include <type_traits>
#include <lsp/json/json.h>

namespace lsp{

namespace messages{
enum class Method;
}

/*
 * Message
 */

using MessagePtr = std::unique_ptr<struct Message>;

struct Message{
	virtual ~Message() = default;
	virtual messages::Method method() const = 0;

	template<typename T>
	requires std::derived_from<T, Message>
	bool isA() const{
		return dynamic_cast<const T*>(this) != nullptr;
	}

	template<typename T>
	requires std::derived_from<T, Message>
	T& as(){
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	requires std::derived_from<T, Message>
	const T& as() const{
		return dynamic_cast<T&>(*this);
	}

	static MessagePtr createFromMethod(messages::Method method); // generated implementation in messages.cpp
};

struct ClientToServerMessage : virtual Message{};
struct ServerToClientMessage : virtual Message{};
struct BidirectionalMessage : ClientToServerMessage, ServerToClientMessage{};

/*
 * Interfaces for serialization/deserialization of message params and result
 */

struct ParamsMessage{
	virtual void initParams(const json::Any&){}
	virtual json::Any paramsJson() const{ return {}; };
};

struct ResultMessage{
	virtual void initResult(const json::Any&){}
	virtual json::Any resultJson() const{ return {}; }
};

/*
 * Request
 */

struct ClientToServerRequest : ClientToServerMessage, virtual ParamsMessage, virtual ResultMessage{};
struct ServerToClientRequest : ServerToClientMessage, virtual ParamsMessage, virtual ResultMessage{};
struct BidirectionalRequest : ClientToServerRequest, ServerToClientRequest{};

/*
 * Notification
 */

struct ClientToServerNotification : ClientToServerMessage, virtual ParamsMessage{};
struct ServerToClientNotification : ServerToClientMessage, virtual ParamsMessage{};
struct BidirectionalNotification : ClientToServerNotification, ServerToClientNotification{};

}
