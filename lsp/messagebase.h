#pragma once

#include <string_view>

namespace lsp{

// Generated
enum class MessageMethod;
std::string_view messageMethodToString(MessageMethod method);
MessageMethod messageMethodFromString(std::string_view str);

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

} // namespace message

} // namespace lsp
