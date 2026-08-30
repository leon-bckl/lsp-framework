#pragma once

#include <atomic>
#include <lsp/io/stream.h>
#include <lsp/messagehandler.h>

namespace lsp{
namespace requests{

struct Initialize;
struct Shutdown;
struct Exit;

} // namespace requests

/*
 * EndpointBase
 */

class EndpointBase{
public:
	EndpointBase(io::Stream& stream);

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
	template<typename M>
	void preMethodCall(){ verifyInitialized(); }

	template<typename M>
	void postMethodCall(){}

	template<>
	void preMethodCall<requests::Initialize>();

	template<>
	void preMethodCall<requests::Shutdown>();

	template<>
	void preMethodCall<requests::Exit>();

private:
	void verifyInitialized() const;
};

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

	template<>
	void preMethodCall<requests::Initialize>();

	template<>
	void postMethodCall<requests::Initialize>();

	template<>
	void preMethodCall<requests::Shutdown>();

	template<>
	void preMethodCall<requests::Exit>();

private:
	void verifyInitialized() const;
};

} // namespace lsp
