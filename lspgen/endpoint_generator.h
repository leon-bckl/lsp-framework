#pragma once

#include "cpp_writer.h"
#include "generator.h"

namespace lspgen{

struct Message;
class MetaModel;

class EndpointGenerator : public Generator{
public:
	enum class Direction{
		ClientToServer,
		ServerToClient
	};

	void generate(const MetaModel& metaModel, Direction direction, const std::string& fileBaseName);

private:
	const MetaModel* m_metaModel = nullptr;
	CppWriter        m_declWriter;
	CppWriter        m_implWriter;

	void generateMethods(const std::string& className, Direction direction);
	void generateOutgingMethod(const std::string& className, const std::string& method, const Message& message, CppWriter& inlineImplWriter);
	void generateIncomingMethod(const std::string& className, const std::string& method, const Message& message);
};

} // namespace lspgen
