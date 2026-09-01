#include <cassert>
#include "endpoint_generator.h"
#include "meta_model.h"

namespace lspgen{

void EndpointGenerator::generate(const MetaModel& metaModel, Direction direction)
{
	m_metaModel = &metaModel;
	m_declWriter.reset();
	m_implWriter.reset();

	m_declWriter.write(
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <lsp/endpoint_base.h>
#include <lsp/message_handler.h>
#include <lsp/messages.h>
#include <lsp/request_result.h>
#include <lsp/types.h>

)", false);

	m_implWriter.write(
R"(/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

)", false);

	const auto className  = std::string(direction == Direction::ClientToServer ? "ClientEndpoint" : "ServerEndpoint");
	const auto headerName = std::string(direction == Direction::ClientToServer ? "client_endpoint" : "server_endpoint");

	m_implWriter.writeLine("#include \"" + headerName + ".h\"", false);
	m_implWriter.writeEmptyLine();
	m_implWriter.writeNamespaceStart("lsp");

	m_declWriter.writeNamespaceStart("lsp");
	m_declWriter.writeClassStart(className, className + "Base");
	m_declWriter.outdent();
	m_declWriter.writeLine("public:");
	m_declWriter.indent();
	m_declWriter.writeFuncSig(className, {}, {{"io::Stream&", "stream"}});
	m_declWriter.writeLine(";");
	m_declWriter.writeEmptyLine();

	m_implWriter.writeFuncSig(className + "::" + className, {}, {{"io::Stream&", "stream"}});
	m_implWriter.writeLine({});
	m_implWriter.indent();
	m_implWriter.write(": " + className + "Base{stream}");
	m_implWriter.outdent();
	m_implWriter.writeBlockStart(true);
	m_implWriter.writeBlockEnd(false, true);

	generateMethods(className, direction);

	m_declWriter.writeClassEnd();
	m_declWriter.writeNamespaceEnd("lsp", false);

	m_implWriter.writeNamespaceEnd("lsp", false);
}

auto EndpointGenerator::headerText() const -> std::string
{
	return std::string(m_declWriter.text());
}

auto EndpointGenerator::sourceText() const -> std::string
{
	return std::string(m_implWriter.text());
}

void EndpointGenerator::generateMethods(const std::string& className, Direction direction)
{
	const auto outgoingDirection =
		direction == Direction::ClientToServer ? Message::Direction::ClientToServer : Message::Direction::ServerToClient;
	const auto incomingDirection =
		direction == Direction::ClientToServer ? Message::Direction::ServerToClient : Message::Direction::ClientToServer;

	// Outgoing methods
	{
		auto inlineImplWriter = CppWriter(1);

		m_declWriter.writeDocComment("Outgoing requests", {});
		m_declWriter.writeEmptyLine();

		for(const auto& [method, message] : m_metaModel->messagesByType(MetaModel::MessageType::Request))
		{
			if(message.direction == outgoingDirection)
				generateOutgingMethod(className, method, message, inlineImplWriter);
		}

		m_declWriter.writeEmptyLine();
		m_declWriter.write(inlineImplWriter.text(), false);
		inlineImplWriter.reset();

		m_declWriter.writeDocComment("Outgoing notifications", {});
		m_declWriter.writeEmptyLine();

		for(const auto& [method, message] : m_metaModel->messagesByType(MetaModel::MessageType::Notification))
		{
			if(message.direction == outgoingDirection)
				generateOutgingMethod(className, method, message, inlineImplWriter);
		}
	}

	// Incoming methods
	{
		m_declWriter.writeEmptyLine();
		m_declWriter.writeDocComment("Incoming requests", {});
		m_declWriter.writeEmptyLine();

		for(const auto& [method, message] : m_metaModel->messagesByType(MetaModel::MessageType::Request))
		{
			if(message.direction == incomingDirection)
				generateIncomingMethod(className, method, message);
		}

		m_declWriter.writeDocComment("Incoming notifications", {});
		m_declWriter.writeEmptyLine();

		for(const auto& [method, message] : m_metaModel->messagesByType(MetaModel::MessageType::Notification))
		{
			if(message.direction == incomingDirection)
				generateIncomingMethod(className, method, message);
		}
	}
}

void EndpointGenerator::generateOutgingMethod(const std::string& className, const std::string& method, const Message& message, CppWriter& inlineImplWriter)
{
	const auto methodName     = CppWriter::lowerCaseIdentifier(method);
	const auto isNotification = message.resultTypeName.empty();
	const auto messageType    = (isNotification ? "notifications::" : "requests::")
	                            + CppWriter::upperCaseIdentifier(method);
	const auto returnType     = isNotification
	                            ? "void"
	                            : "RequestResult<" + CppWriter::upperCaseIdentifier(message.resultTypeName) + ">";
	const auto hasParams      = !message.paramsTypeName.empty();
	auto       funcParams     = CppWriter::FuncParamList();

	if(hasParams)
		funcParams.push_back({CppWriter::type(message.paramsTypeName, CppWriter::TypeConst | CppWriter::TypeRef), "params"});

	if(!isNotification)
		m_declWriter.write("[[nodiscard]] ");

	m_declWriter.writeFuncSig(methodName, returnType, funcParams);
	m_declWriter.writeLine(";");

	m_implWriter.writeFuncSig(className + "::" + methodName, returnType, funcParams);
	m_implWriter.writeBlockStart(true);
	m_implWriter.writeLine("const auto hook = messageHook<" + messageType + ">(*this);");

	if(isNotification)
		m_implWriter.writeLine("messageHandler().sendNotification<" + messageType + ">(" + (hasParams ? "params" : "") + ");");
	else
		m_implWriter.writeLine("return messageHandler().sendRequest<" + messageType + ">(" + (hasParams ? "params" : "") + ");");

	m_implWriter.writeBlockEnd(false, true);

	if(!isNotification)
	{
		inlineImplWriter.writeLine("template<typename F, typename E = MessageHandler::ResponseErrorCallback>");
		funcParams.push_back({"F&&", "then"});
		funcParams.push_back({"E&&", "error", "nullError"});
		inlineImplWriter.writeFuncSig(methodName, "MessageId", funcParams);
		inlineImplWriter.writeBlockStart(true);
		inlineImplWriter.writeLine("const auto hook = messageHook<" + messageType + ">(*this);");

		if(hasParams)
			inlineImplWriter.writeLine("return messageHandler().sendRequest<" + messageType + ">(params, std::forward<F>(then), std::forward<E>(error));");
		else
			inlineImplWriter.writeLine("return messageHandler().sendRequest<" + messageType + ">(std::forward<F>(then), std::forward<E>(error));");

		inlineImplWriter.writeBlockEnd(false, true);
	}
}

void EndpointGenerator::generateIncomingMethod(const std::string& className, const std::string& method, const Message& message)
{
	const auto isNotification = message.resultTypeName.empty();
	const auto messageType    = (isNotification ? "notifications::" : "requests::") + CppWriter::upperCaseIdentifier(method);
	const auto hasParams      = !message.paramsTypeName.empty();
	const auto paramsType     = CppWriter::upperCaseIdentifier(message.paramsTypeName);
	const auto hasResult      = !message.resultTypeName.empty();

	m_declWriter.writeLine("template<typename F>");
	m_declWriter.writeFuncSig("on" + CppWriter::upperCaseIdentifier(method), CppWriter::type(className, CppWriter::TypeRef), {{"F&&", "callback"}});
	m_declWriter.writeBlockStart(true);
	m_declWriter.writeLine("messageHandler().on<" + messageType + ">(");
	m_declWriter.indent();
	m_declWriter.write("[this, callback = std::forward<F>(callback)](");

	if(hasParams)
		m_declWriter.write(paramsType + "&& params");

	m_declWriter.write(") mutable");
	m_declWriter.writeBlockStart(true);
	m_declWriter.writeLine("const auto hook = messageHook<" + messageType + ">(*this);");

	if(hasResult)
		m_declWriter.write("return ");

	if(hasParams)
		m_declWriter.writeLine("callback(std::move(params));");
	else
		m_declWriter.writeLine("callback();");

	m_declWriter.writeBlockEnd(false, false);
	m_declWriter.outdent();
	m_declWriter.writeLine(");");
	m_declWriter.writeLine("return *this;");
	m_declWriter.writeBlockEnd(false, true);
}

} // namespace lspgen
