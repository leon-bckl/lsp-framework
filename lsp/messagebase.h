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
	virtual void initParams(const json::Any&){}
	virtual void initResult(const json::Any&){}
	virtual json::Any paramsJson(){ return {}; };
	virtual json::Any resultJson(){ return {}; }

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

struct BidirectionalMessage : virtual ClientToServerMessage, virtual ServerToClientMessage{};

/*
 * Request
 */

struct ClientToServerRequest : virtual ClientToServerMessage{};

struct ServerToClientRequest : virtual ServerToClientMessage{};

struct BidirectionalRequest : virtual ClientToServerRequest, virtual ServerToClientRequest{};

/*
 * Notification
 */

struct ClientToServerNotification : virtual ClientToServerMessage{};

struct ServerToClientNotification : virtual ServerToClientMessage{};

struct BidirectionalNotification : virtual ClientToServerNotification, virtual ServerToClientNotification{};

}
