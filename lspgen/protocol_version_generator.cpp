#include <stdexcept>
#include "protocol_version_generator.h"
#include "meta_model.h"
#include "util.h"

namespace lspgen{

void ProtocolVersionGenerator::generate(const MetaModel& metaModel, const std::string& fileBaseName)
{
	m_metaModel = &metaModel;
	m_versionWriter.reset(*createFile(fileBaseName + ".h"));
	m_versionWriter.write(
R"(#pragma once

#include <lsp/version.h>

)", false);

	const auto versionParts = splitStringView(m_metaModel->metaData().version, ".");

	if(versionParts.size() != 3)
		throw std::runtime_error("Expected metaData.version to have the format \"x.x.x\"");

	m_versionWriter.write("#define LSP_PROTOCOL_VERSION_MAJOR ");
	m_versionWriter.writeLine(versionParts[0]);
	m_versionWriter.write("#define LSP_PROTOCOL_VERSION_MINOR ");
	m_versionWriter.writeLine(versionParts[1]);
	m_versionWriter.write("#define LSP_PROTOCOL_VERSION_PATCH ");
	m_versionWriter.writeLine(versionParts[2]);
	m_versionWriter.writeLine("#define LSP_PROTOCOL_VERSION \\\n"
		"\tLSP_INT_VERSION(LSP_PROTOCOL_VERSION_MAJOR, LSP_PROTOCOL_VERSION_MINOR, LSP_PROTOCOL_VERSION_PATCH)");
	m_versionWriter.writeLine("#define LSP_PROTOCOL_VERSION_STR\\\n"
		"\tLSP_STRINGIFY_VERSION(LSP_PROTOCOL_VERSION_MAJOR, LSP_PROTOCOL_VERSION_MINOR, LSP_PROTOCOL_VERSION_PATCH)");
}

} // namespace lspgen
