#include <stdexcept>
#include "endpoint_base.h"

namespace lsp{

/*
 * EndpointBase
 */

EndpointBase::EndpointBase(io::Stream& stream)
	: m_messageHandler{Connection(stream)}
{
	setState(State::Uninitialized);
}

auto EndpointBase::isActive() const -> bool
{
	return state() != State::Inactive;
}

auto EndpointBase::messageHandler() -> MessageHandler&
{
	return m_messageHandler;
}

void EndpointBase::processNextMessage()
{
	try
	{
		messageHandler().processNextMessage();
	}
	catch(const ConnectionError&)
	{
		// Ignore connection error when inactive since no more messages are expected
		if(isActive())
		{
			setState(State::Inactive);
			throw;
		}
	}
}

void EndpointBase::runMessageLoop()
{
	while(isActive())
		processNextMessage();
}

auto EndpointBase::state() const -> State
{
	return m_state.load();
}

void EndpointBase::setState(State state)
{
	m_state.store(state);
}

/*
 * ClientEndpointBase
 */

ClientEndpointBase::ClientEndpointBase(io::Stream& stream)
	: EndpointBase{stream}
{
}

void ClientEndpointBase::verifyInitialized() const
{
	const auto currentState = state();

	if(currentState <= State::Uninitialized)
		throw std::logic_error("Initialize request must be sent first");

	if(currentState == State::Shutdown)
		throw std::logic_error("Only 'exit' request must be sent after 'shutdown'");
}

/*
 * ServerEndpointBase
 */

ServerEndpointBase::ServerEndpointBase(io::Stream& stream)
	: EndpointBase{stream}
{
}

auto ServerEndpointBase::isInitialized() const -> bool
{
	return state() > State::Uninitialized;
}

} // namespace lsp
