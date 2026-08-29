#pragma once

#include <lsp/json/json.h>

namespace lsp{

/*
 * Message
 */

enum class MessageKind{
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
	static constexpr auto Kind      = MessageKind::Request;
	static constexpr auto Direction = MessageDirection::Bidirectional;
	using Params = json::Value;
	using Result = json::Value;
};

struct GenericRequestNoParams{
	static constexpr auto Kind      = MessageKind::Request;
	static constexpr auto Direction = MessageDirection::Bidirectional;
	using Result = json::Value;
};

struct GenericNotification{
	static constexpr auto Kind      = MessageKind::Notification;
	static constexpr auto Direction = MessageDirection::Bidirectional;
	using Params = json::Value;
};

struct GenericNotificationNoParams{
	static constexpr auto Kind      = MessageKind::Notification;
	static constexpr auto Direction = MessageDirection::Bidirectional;
};

} // namespace lsp
