#pragma once

#include <atomic>
#include <lsp/io/stream.h>
#include <lsp/message_handler.h>

namespace lsp{
namespace requests{
struct Initialize;
struct Shutdown;
} // namespace requests

namespace notifications{
struct Exit;
} // namespace notifications

using RequestContext = MessageHandler::RequestContext;

/*
 * EndpointBase
 */

class EndpointBase{
public:
	EndpointBase(io::Stream& stream);

	auto isActive() const -> bool;
	auto messageHandler() -> MessageHandler&;
	void processNextMessage();
	void runMessageLoop();

	static void nullError(const ResponseError&){}

protected:
	enum class State{
		Inactive,      // Initial state or exit notification received
		Uninitialized, // Started up and waiting for initialize request
		Active,        // Currently handling requests
		Shutdown       // Shutdown notification received
	};

	auto state() const -> State;
	void setState(State state);

	// Message hook to call pre and post method call functions
	template<typename MessageType, typename EndpointType>
	class MessageHook{
	public:
		MessageHook(EndpointType& endpoint)
			: m_endpoint{&endpoint}
		{
			m_endpoint->template preMethodCall<MessageType>();
		}

		~MessageHook()
		{
			m_endpoint->template postMethodCall<MessageType>();
		}

	private:
		EndpointType* m_endpoint = nullptr;
	};

	template<typename MessageType, typename EndpointType>
	[[nodiscard]] static auto messageHook(EndpointType& endpoint) -> MessageHook<MessageType, EndpointType>
	{
		return MessageHook<MessageType, EndpointType>(endpoint);
	}

private:
	std::atomic<State> m_state = State::Inactive;
	MessageHandler     m_messageHandler;
};

/*
 * ClientEndpointBase
 */

class ClientEndpointBase : public EndpointBase{
public:
	ClientEndpointBase(io::Stream& stream);

	template<typename M>
	void preMethodCall(){ verifyInitialized(); }

	template<typename M>
	void postMethodCall(){}

private:
	void verifyInitialized() const;
};

template<>
inline void ClientEndpointBase::preMethodCall<requests::Initialize>(){}

template<>
inline void ClientEndpointBase::postMethodCall<requests::Initialize>()
{
	setState(State::Active);
}

template<>
inline void ClientEndpointBase::preMethodCall<requests::Shutdown>()
{
	verifyInitialized();
	setState(State::Shutdown);
}

template<>
inline void ClientEndpointBase::preMethodCall<notifications::Exit>()
{
	setState(State::Inactive);
}

/*
 * ServerEndpointBase
 */

class ServerEndpointBase : public EndpointBase{
public:
	ServerEndpointBase(io::Stream& stream);

	auto isInitialized() const -> bool;

	template<typename M>
	void preMethodCall(){ verifyInitialized(); }

	template<typename M>
	void postMethodCall(){}

private:
	void verifyInitialized() const;
};

template<>
inline void ServerEndpointBase::preMethodCall<requests::Initialize>()
{
	if(isInitialized())
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server already initialized");
}

template<>
inline void ServerEndpointBase::postMethodCall<requests::Initialize>()
{
	if(state() == State::Uninitialized)
		setState(State::Active);
}

inline void ServerEndpointBase::verifyInitialized() const
{
	const auto currentState = state();

	if(currentState <= State::Uninitialized)
		throw lsp::RequestError(lsp::MessageError::ServerNotInitialized, "Server not initialized");

	if(currentState == State::Shutdown)
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server has received shutdown request");
}

template<>
inline void ServerEndpointBase::preMethodCall<requests::Shutdown>()
{
	verifyInitialized();
	setState(State::Shutdown);
}

template<>
inline void ServerEndpointBase::preMethodCall<notifications::Exit>()
{
	setState(State::Inactive);
}

} // namespace lsp
