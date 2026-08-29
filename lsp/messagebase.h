#pragma once

#include <lsp/json/json.h>

namespace lsp{

/*
 * Message
 */

enum class Message{
	Notification,
	Request
};

enum class MessageDirection{
	ClientToServer,
	ServerToClient,
	Bidirectional
};

/*
 * Generic messages
 */

struct GenericRequest{
	static constexpr auto Type      = Message::Request;
	static constexpr auto Direction = MessageDirection::Bidirectional;
	using Params = json::Value;
	using Result = json::Value;
};

struct GenericNotification{
	static constexpr auto Type      = Message::Notification;
	static constexpr auto Direction = MessageDirection::Bidirectional;
	using Params = json::Value;
};

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

template<typename T>
concept IsRequest = T::Type == Message::Request;

template<typename T>
concept IsNotification = T::Type == Message::Notification;

} // namespace message

} // namespace lsp
