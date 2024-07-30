#pragma once

#include <string_view>

namespace lsp{

enum class MessageMethod;
std::string_view messageMethodToString(MessageMethod method);
MessageMethod messageMethodFromString(std::string_view str);

/*
 * Message
 */

struct Message{
	Message() = delete;
	virtual ~Message() = default;
};

struct ClientToServerMessage : virtual Message{};
struct ServerToClientMessage : virtual Message{};
struct BidirectionalMessage : ClientToServerMessage, ServerToClientMessage{};

/*
 * Request
 */

struct ClientToServerRequest : ClientToServerMessage{};
struct ServerToClientRequest : ServerToClientMessage{};
struct BidirectionalRequest : ClientToServerRequest, ServerToClientRequest{};

/*
 * Notification
 */

struct ClientToServerNotification : ClientToServerMessage{};
struct ServerToClientNotification : ServerToClientMessage{};
struct BidirectionalNotification : ClientToServerNotification, ServerToClientNotification{};

/*
 * Concepts
 */

namespace message{

template<typename T>
concept HasParams = requires
{
	typename T::Params;
};

template<typename T>
concept HasResult = requires
{
	typename T::Result;
};

template<typename T>
concept HasPartialResult = requires
{
	typename T::PartialResult;
};

} // namespace message

} // namespace lsp
