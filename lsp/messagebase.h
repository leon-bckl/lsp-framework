#pragma once

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
