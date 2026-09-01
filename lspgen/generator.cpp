#include "generator.h"
#include "util.h"

namespace lspgen{

auto Generator::createFile(std::string_view fileName) -> std::string*
{
	auto& content = m_files[std::string(fileName)];

	content =
R"(/*#############################################################
 * NOTE: This is a generated file and it shouldn't be modified!
 *#############################################################*/

)";

	return &content;
}

void Generator::writeFiles() const
{
	for(const auto& [file, content] : m_files)
		writeFileContentIfNotSame(file, content);
}

} // namespace lspgen
