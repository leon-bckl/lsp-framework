#pragma once

#include <string>
#include "cppwriter.h"

namespace lspgen{

class MetaModel;
struct Message;

class MessageGenerator{
public:
	void generate(const MetaModel& metaModel);
	auto headerText() const -> std::string;

private:
	const MetaModel* m_metaModel = nullptr;
	CppWriter        m_messageWriter;

	void generateMessage(std::string_view method, const Message& message, bool isNotification);
};

} // namespace lspgen
