#include <cstdlib>
#include <iostream>
#include <lsp/json/json.h>
#include "endpoint_generator.h"
#include "message_generator.h"
#include "meta_model.h"
#include "protocol_version_generator.h"
#include "type_generator.h"
#include "util.h"

using namespace lsp;
using namespace lspgen;

namespace{
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
			protocolVersionGenerator.generate(metaModel, "protocol_version");
			protocolVersionGenerator.writeFiles();
		}

		{
			auto typeGenerator = TypeGenerator();
			typeGenerator.generate(metaModel,  "types");
			typeGenerator.writeFiles();
		}

		{
			auto messageGenerator = MessageGenerator();
			messageGenerator.generate(metaModel, "messages");
			messageGenerator.writeFiles();
		}

		{
			auto endpointGenerator = EndpointGenerator();
			endpointGenerator.generate(metaModel, EndpointGenerator::Direction::ServerToClient, "server_endpoint");
			endpointGenerator.generate(metaModel, EndpointGenerator::Direction::ClientToServer, "client_endpoint");
			endpointGenerator.writeFiles();
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
