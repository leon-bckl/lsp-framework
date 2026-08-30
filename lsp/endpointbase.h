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

	static void nullError(const ResponseError&){}

protected:
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
	MessageHandler m_messageHandler;
};

/*
 * ClientEndpointBase
 */

class ClientEndpointBase : public EndpointBase{
protected:
	template<typename M>
	void preMethodCall(){}

	template<typename M>
	void postMethodCall(){}
};

/*
 * ServerEndpointBase
 */

class ServerEndpointBase : public EndpointBase{
public:
	ServerEndpointBase(io::Stream& stream);

	auto isInitialized() const -> bool;

protected:
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
	enum class State{
		Uninitialized, // Started up and waiting for initialize request
		Active,        // Currently handling requests
		Shutdown       // Shutdown notification received
	};

	std::atomic<State> m_state = State::Uninitialized;

	void verifyInitialized() const;
};

} // namespace lsp
