#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lsp/json/json.h>
#include "messagegenerator.h"
#include "protocolversiongenerator.h"
#include "typegenerator.h"
#include "metamodel.h"

using namespace lsp;
using namespace lspgen;

namespace{

auto readFileContent(const std::string& fileName) -> std::string
{
	auto file = std::ifstream(fileName, std::ios::binary);

	if(!file)
		throw std::runtime_error("Failed to read file '" + fileName + '\'');

	file.seekg(0, std::ios::end);
	const auto fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string text;
	text.resize(static_cast<std::string::size_type>(fileSize));
	file.read(&text[0], fileSize);

	return text;
}

auto writeFileContent(const std::string& fileName, std::string_view content)
{
	auto file = std::ofstream(fileName, std::ios::trunc | std::ios::binary);

	if(!file)
		throw std::runtime_error("Failed to write file '" + fileName + '\'');

	file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

} // namespace

auto main(int argc, char** argv) -> int
{
	if(argc != 2)
	{
		std::cerr << "Expected the input file name as the first and only argument" << std::endl;
		return EXIT_FAILURE;
	}

	int exitCode = EXIT_SUCCESS;

	try
	{
		const auto metaModel = MetaModel(json::parse(readFileContent(argv[1])).object());

		{
			auto protocolVersionGenerator = ProtocolVersionGenerator();
			protocolVersionGenerator.generate(metaModel);

			writeFileContent("protocolversion.h", protocolVersionGenerator.headerText());
		}

		{
			auto typeGenerator = TypeGenerator();
			typeGenerator.generate(metaModel);

			writeFileContent("types.h", typeGenerator.headerText());
			writeFileContent("types.cpp", typeGenerator.sourceText());
		}

		{
			auto messageGenerator = MessageGenerator();
			messageGenerator.generate(metaModel);

			writeFileContent("messages.h", messageGenerator.headerText());
		}
	}
	catch(const json::ParseError& e)
	{
		std::cerr << "JSON parse error at offset " << e.textPos() << ": " << e.what() << std::endl;
		exitCode = EXIT_FAILURE;
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		exitCode = EXIT_FAILURE;
	}
	catch(...)
	{
		std::cerr << "Unknown error" << std::endl;
		exitCode = EXIT_FAILURE;
	}

	return exitCode;
}
