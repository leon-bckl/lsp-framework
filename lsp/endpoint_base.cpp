#include <stdexcept>
#include <lsp/types.h>
#include "endpoint_base.h"

namespace lsp{

/*
 * EndpointBase
 */

EndpointBase::EndpointBase(io::Stream& stream)
	: m_messageHandler(Connection(stream))
{
	setState(State::Uninitialized);

	messageHandler().addMessageLogCallback(
		[this](const MessageHandler::MessageLog& msgLog)
		{
			if(!m_logHook || msgLog.method == "$/logTrace")
				return;

			const auto idString = [&id = msgLog.id]() -> std::string
			{
				if(!id.has_value())
					return {};

				if(std::holds_alternative<json::Null>(*id))
					return "null";

				if(const auto* intId = std::get_if<json::Integer>(&*id))
					return std::to_string(*intId);

				return std::get<json::String>(*id);
			}();

			const auto durationStr = [&msgLog]() -> std::string
			{
				if(!msgLog.requestDuration.has_value())
					return {};

				const auto duration =
					std::chrono::duration_cast<std::chrono::milliseconds>(*msgLog.requestDuration);
				return std::to_string(duration.count()) + "ms";
			}();

			auto message = std::string();

			if(msgLog.incoming)
				message += "Received";
			else
				message += "Sending";

			if(msgLog.isNotification())
				message += " notification";
			else if(msgLog.isRequest())
				message += " request";
			else if(msgLog.isResponse())
				message += " response";
			else
				message += " message";

			message += " '";
			message += msgLog.method;

			if(msgLog.id.has_value())
				message += " - (" + idString + ")";

			message += '\'';

			if(msgLog.isResponse())
			{
				if(msgLog.incoming)
					message += " in " + durationStr + '.';
				else
					message += ". Processing request took " + durationStr;
			}
			else
			{
				message += ".";
			}

			if(msgLog.isResponse() && msgLog.incoming && msgLog.error.has_value())
			{
				message +=
					" Request failed: " +
					std::string(msgLog.error->message) +
					"(" +
					std::to_string(msgLog.error->code) +
					").";
			}

			auto verbose = std::string();

			if(msgLog.payload.has_value())
			{
				if(msgLog.isResponse())
				{
					if(msgLog.error.has_value())
						verbose += "Error data";
					else
						verbose += "Result";
				}
				else
				{
					verbose += "Params";
				}

				verbose += ": " + *msgLog.payload;
			}

			m_logHook(message, verbose);
		});
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

void EndpointBase::customNotification(std::string_view method, const json::Value& params)
{
	messageHandler().sendCustomNotification<GenericNotification>(method, params);
}

void EndpointBase::customNotification(std::string_view method)
{
	messageHandler().sendCustomNotification<GenericNotificationNoParams>(method);
}

void EndpointBase::setLogHook(LogHook hook)
{
	m_logHook = std::move(hook);
}

/*
 * ClientEndpointBase
 */

ClientEndpointBase::ClientEndpointBase(io::Stream& stream)
	: EndpointBase(stream)
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

template<>
void ClientEndpointBase::preMethodCall<requests::Initialize, InitializeParams>(const InitializeParams&)
{
}

template<>
void ClientEndpointBase::postMethodCall<requests::Initialize>()
{
	setState(State::Active);
}

template<>
void ClientEndpointBase::preMethodCall<requests::Shutdown>()
{
	verifyInitialized();
	setState(State::Shutdown);
}

template<>
void ClientEndpointBase::preMethodCall<notifications::Exit>()
{
	setState(State::Inactive);
}

/*
 * ServerEndpointBase
 */

ServerEndpointBase::ServerEndpointBase(io::Stream& stream)
	: EndpointBase(stream)
{
	registerBaseHandlers();
}

auto ServerEndpointBase::isInitialized() const -> bool
{
	return state() > State::Uninitialized;
}

template<>
void ServerEndpointBase::preMethodCall<requests::Initialize, InitializeParams>(const InitializeParams& params)
{
	if(isInitialized())
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server already initialized");

	if(params.trace.has_value())
	{
		switch(*params.trace)
		{
		case TraceValue::Off:
			messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::Off);
			break;
		case TraceValue::Messages:
			messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
			break;
		case TraceValue::Verbose:
			messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::InfoAndPayload);
			break;
		default:
			break;
		}
	}
}

template<>
void ServerEndpointBase::postMethodCall<requests::Initialize>()
{
	if(state() == State::Uninitialized)
		setState(State::Active);
}

void ServerEndpointBase::verifyInitialized() const
{
	const auto currentState = state();

	if(currentState <= State::Uninitialized)
		throw lsp::RequestError(lsp::MessageError::ServerNotInitialized, "Server not initialized");

	if(currentState == State::Shutdown)
		throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Server has received shutdown request");
}

template<>
void ServerEndpointBase::preMethodCall<requests::Shutdown>()
{
	verifyInitialized();
	setState(State::Shutdown);
}

template<>
void ServerEndpointBase::preMethodCall<notifications::Exit>()
{
	setState(State::Inactive);
}

void ServerEndpointBase::registerBaseHandlers()
{
	setLogHook(
		[this](std::string_view message, std::string_view verbose)
		{
			auto params = json::Object();

			params.append("message", message);

			if(!verbose.empty())
				params.append("verbose", verbose);

			customNotification("$/logTrace", params);
		});

	onCustomNotification("$/setTrace",
		[this](const json::Value& params)
		{
			if(!params.isObject())
				return;

			const auto* traceValue = params.object().find("value");

			if(!traceValue || !traceValue->isString())
				return;

			const auto& traceStr = traceValue->string();

			if(traceStr == "off")
				messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::Off);
			else if(traceStr == "messages")
				messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
			else if(traceStr == "verbose")
				messageHandler().setMessageLogLevel(MessageHandler::MessageLogLevel::InfoAndPayload);
		});

}

} // namespace lsp
