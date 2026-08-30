#include "endpointbase.h"

namespace lsp{

/*
 * Endpoint
 */

EndpointBase::EndpointBase(io::Stream& stream)
	: m_messageHandler{Connection(stream)}
{
}

auto EndpointBase::messageHandler() -> MessageHandler&
{
	return m_messageHandler;
}

/*
 * ServerEndpoint
 */

ServerEndpointBase::ServerEndpointBase(io::Stream& stream)
	: EndpointBase{stream}
{
}

auto ServerEndpointBase::isInitialized() const -> bool
{
	return m_state.load() > State::Uninitialized;
}

template<>
void ServerEndpointBase::preMethodCall<requests::Initialize>()
{
	if(isInitialized())
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server already initialized");
}

template<>
void ServerEndpointBase::postMethodCall<requests::Initialize>()
{
	if(m_state.load() == State::Uninitialized)
		m_state.store(State::Active);
}

void ServerEndpointBase::verifyInitialized() const
{
	const auto state = m_state.load();

	if(state <= State::Uninitialized)
		throw lsp::RequestError(lsp::MessageError::ServerNotInitialized, "Server not initialized");

	if(state == State::Shutdown)
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server has received shutdown request");
}

template<>
void ServerEndpointBase::preMethodCall<requests::Shutdown>()
{
	verifyInitialized();
	m_state.store(State::Shutdown);
}

template<>
void ServerEndpointBase::preMethodCall<requests::Exit>()
{
	verifyInitialized();
	m_state.store(State::Uninitialized);
}

} // namespace lsp
