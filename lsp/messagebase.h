#pragma once

#include <memory>
#include <type_traits>

namespace lsp{
namespace json{
class Any;
}

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
	T& as(){
		return dynamic_cast<T&>(*this);
	}

	template<typename T>
	requires std::derived_from<T, Message>
	const T& as() const{
		return dynamic_cast<T&>(*this);
	}

	static MessagePtr createFromMethod(messages::Method method);
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
