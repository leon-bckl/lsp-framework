#pragma once

#include <string>
#include "cpp_writer.h"

namespace lspgen{

class MetaModel;

class ProtocolVersionGenerator{
public:
	void generate(const MetaModel& metaModel);
	auto headerText() const -> std::string;

private:
	const MetaModel* m_metaModel = nullptr;
	CppWriter        m_versionWriter;
};

} // namespace lspgen
