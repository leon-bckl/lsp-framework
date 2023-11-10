#pragma once

#include <memory>

namespace lsp{
namespace json{

class Any;

}

/*
 * Message
 */

enum class MessageMethod;

using MessagePtr = std::unique_ptr<struct Message>;

struct Message{
	virtual ~Message() = default;
	virtual MessageMethod method() const = 0;

	template<typename T>
	T& as(){
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	const T& as() const{
		return dynamic_cast<T&>(*this);
	}

	static MessagePtr createFromMethod(MessageMethod method);
};

struct ClientToServerMessage : virtual Message{
	virtual void initParams(const json::Any& json) = 0;
};

struct ServerToClientMessage : virtual Message{
	virtual json::Any paramsJson() = 0;
};

struct BidirectionalMessage : ClientToServerMessage, ServerToClientMessage{

};

/*
 * Request
 */

struct ClientToServerRequest : ClientToServerMessage{
	virtual json::Any resultJson() = 0;
};

struct ServerToClientRequest : ServerToClientMessage{

};

struct BidirectionalRequest : BidirectionalMessage{
	virtual json::Any resultJson() = 0;
};

/*
 * Notification
 */

struct ClientToServerNotification : ClientToServerMessage{

};

struct ServerToClientNotification : ServerToClientMessage{

};

struct BidirectionalNotification : BidirectionalMessage{

};

}
