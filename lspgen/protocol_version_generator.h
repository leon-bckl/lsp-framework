#pragma once

#include <string>
#include "cpp_writer.h"
#include "generator.h"

namespace lspgen{

class MetaModel;

class ProtocolVersionGenerator : public Generator{
public:
	void generate(const MetaModel& metaModel, const std::string& fileBaseName);

private:
	const MetaModel* m_metaModel = nullptr;
	CppWriter        m_versionWriter;
};

} // namespace lspgen
