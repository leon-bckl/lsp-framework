#include "messagegenerator.h"
#include "metamodel.h"

namespace lspgen{
namespace{

static constexpr auto MessagesHeaderBegin = std::string_view(
R"(#pragma once

/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

#include <string_view>
#include <lsp/messagebase.h>
#include <lsp/types.h>

namespace lsp{

)");

static constexpr auto MessagesHeaderEnd = std::string_view(
R"(} // namespace lsp
)");


} // namespace

void MessageGenerator::generate(const MetaModel& metaModel)
{
	m_metaModel = &metaModel;
	m_messageWriter.reset();

	m_messageWriter.writeDocComment("Request messages", {});
	m_messageWriter.writeEmptyLine();
	m_messageWriter.writeNamespaceStart("requests");

	for(const auto& [method, message] : metaModel.messagesByType(MetaModel::MessageType::Request))
		generateMessage(method, message, false);

	m_messageWriter.writeNamespaceEnd("requests");
	m_messageWriter.writeDocComment("Notification messages", {});
	m_messageWriter.writeEmptyLine();
	m_messageWriter.writeNamespaceStart("notifications");

	for(const auto& [method, message] : metaModel.messagesByType(MetaModel::MessageType::Notification))
		generateMessage(method, message, true);

	m_messageWriter.writeNamespaceEnd("notifications");
}

auto MessageGenerator::headerText() const -> std::string
{
	auto text = std::string();

	text += MessagesHeaderBegin;
	text += m_messageWriter.text();
	text += MessagesHeaderEnd;

	return text;
}

void MessageGenerator::generateMessage(std::string_view method, const Message& message, bool isNotification)
{
	const auto messageCppName   = CppWriter::upperCaseIdentifier(method);
	auto       messageDirection = std::string();

	switch(message.direction)
	{
	case Message::Direction::ClientToServer:
		messageDirection = "ClientToServer";
		break;
	case Message::Direction::ServerToClient:
		messageDirection = "ServerToClient";
		break;
	case Message::Direction::Both:
		messageDirection = "Bidirectional";
	}

	m_messageWriter.writeDocComment(method, message.documentation);
	m_messageWriter.writeStructStart(messageCppName);
	constexpr auto varFlags = CppWriter::VarStatic | CppWriter::VarConstExpr;
	m_messageWriter.writeVariable("Method          ", "auto", "std::string_view(" + json::stringify(method) + ")", varFlags);
	m_messageWriter.writeVariable("ClientCapability", "auto", "std::string_view(" + json::stringify(message.clientCapabilityName) + ")", varFlags);
	m_messageWriter.writeVariable("ServerCapability", "auto", "std::string_view(" + json::stringify(message.serverCapabilityName) + ")", varFlags);
	m_messageWriter.writeVariable("Kind            ", "auto", std::string("MessageKind::") + (isNotification ? "Notification" : "Request"), varFlags);
	m_messageWriter.writeVariable("Direction       ", "auto", "MessageDirection::" + messageDirection, varFlags);

	const bool hasRegistrationOptions = !message.registrationOptionsTypeName.empty();
	const bool hasPartialResult       = !message.partialResultTypeName.empty();
	const bool hasErrorData           = !message.errorDataTypeName.empty();
	const bool hasParams              = !message.paramsTypeName.empty();
	const bool hasResult              = !message.resultTypeName.empty();

	if(hasRegistrationOptions || hasPartialResult || hasParams || hasResult)
		m_messageWriter.writeEmptyLine();

	// Explicitly add lsp namespace in front of types because there are methods that have the same name as a type and might conflict.
	// E.g., WorkspaceSymbol
	const auto typeNamespace = "lsp::";

	if(hasRegistrationOptions)
		m_messageWriter.writeTypedef("RegistrationOptions", typeNamespace + CppWriter::upperCaseIdentifier(message.registrationOptionsTypeName));

	if(hasPartialResult)
		m_messageWriter.writeTypedef("PartialResult", typeNamespace + CppWriter::upperCaseIdentifier(message.partialResultTypeName));

	if(hasErrorData)
		m_messageWriter.writeTypedef("ErrorData", typeNamespace + CppWriter::upperCaseIdentifier(message.errorDataTypeName));

	if(hasParams)
		m_messageWriter.writeTypedef("Params", typeNamespace + CppWriter::upperCaseIdentifier(message.paramsTypeName));

	if(hasResult)
		m_messageWriter.writeTypedef("Result", typeNamespace + CppWriter::upperCaseIdentifier(message.resultTypeName));

	m_messageWriter.writeStructEnd();
}

} // namespace lspgen
