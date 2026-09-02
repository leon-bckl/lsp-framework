#pragma once

#include <string>
#include "cpp_writer.h"
#include "generator.h"

namespace lspgen{

class MetaModel;
struct Message;

class MessageGenerator : public Generator{
public:
	void generate(const MetaModel& metaModel, const std::string& fileBaseName, const std::string& typesHeaderBaseName);

private:
	const MetaModel* m_metaModel = nullptr;
	CppWriter        m_messageWriter;

	void generateMessage(std::string_view method, const Message& message, bool isNotification);
};

} // namespace lspgen
