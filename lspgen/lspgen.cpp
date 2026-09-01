#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lsp/json/json.h>
#include "endpoint_generator.h"
#include "message_generator.h"
#include "meta_model.h"
#include "protocol_version_generator.h"
#include "type_generator.h"

using namespace lsp;
using namespace lspgen;

namespace{

auto readFileContent(const std::string& fileName, bool mayFail = false) -> std::string
{
	auto file = std::ifstream(fileName, std::ios::binary);

	if(!file)
	{
		if(mayFail)
			return {};

		throw std::runtime_error("Failed to read file '" + fileName + '\'');
	}

	file.seekg(0, std::ios::end);
	const auto fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string text;
	text.resize(static_cast<std::string::size_type>(fileSize));
	file.read(&text[0], fileSize);

	return text;
}

void writeFileContent(const std::string& fileName, std::string_view content)
{
	auto file = std::ofstream(fileName, std::ios::trunc | std::ios::binary);

	if(!file)
		throw std::runtime_error("Failed to write file '" + fileName + '\'');

	file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void writeFileContentIfNotSame(const std::string& fileName, std::string_view newFileContent)
{
	const auto oldFileContent = readFileContent(fileName, true);

	if(oldFileContent != newFileContent)
		writeFileContent(fileName, newFileContent);
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

			writeFileContentIfNotSame("protocol_version.h", protocolVersionGenerator.headerText());
		}

		{
			auto typeGenerator = TypeGenerator();
			typeGenerator.generate(metaModel);

			writeFileContentIfNotSame("types.h", typeGenerator.headerText());
			writeFileContentIfNotSame("types.cpp", typeGenerator.sourceText());
		}

		{
			auto messageGenerator = MessageGenerator();
			messageGenerator.generate(metaModel);

			writeFileContentIfNotSame("messages.h", messageGenerator.headerText());
		}

		{
			auto endpointGenerator = EndpointGenerator();

			endpointGenerator.generate(metaModel, EndpointGenerator::Direction::ClientToServer);
			writeFileContentIfNotSame("client_endpoint.h", endpointGenerator.headerText());
			writeFileContentIfNotSame("client_endpoint.cpp", endpointGenerator.sourceText());

			endpointGenerator.generate(metaModel, EndpointGenerator::Direction::ServerToClient);
			writeFileContentIfNotSame("server_endpoint.h", endpointGenerator.headerText());
			writeFileContentIfNotSame("server_endpoint.cpp", endpointGenerator.sourceText());
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
