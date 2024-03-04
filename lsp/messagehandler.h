#pragma once

#include <lsp/requesthandler.h>
#include <lsp/messagedispatcher.h>

namespace lsp{

class MessageHandler{
public:
	MessageHandler(Connection& connection)
		: m_connection{connection},
		  m_requestHandler{connection},
		  m_messageDispatcher{connection}{}

	void processIncomingMessages()
	{
		m_connection.receiveNextMessage(m_requestHandler, m_messageDispatcher);
	}

	RequestHandler& requestHandler(){ return m_requestHandler; }
	MessageDispatcher& messageDispatcher(){ return m_messageDispatcher; }

private:
	Connection&       m_connection;
	RequestHandler    m_requestHandler;
	MessageDispatcher m_messageDispatcher;
};

} // namespace lsp
