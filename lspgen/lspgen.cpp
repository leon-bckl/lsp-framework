#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lsp/json/json.h>
#include "cppgenerator.h"
#include "metamodel.h"

/*
 * This is a huge mess because it started out as an experiment only.
 * But it works and should keep on working with upcoming lsp versions
 * unless there are fundamental changes to the meta model in which case
 * everything should be rewritten from scratch...
 */

using namespace lsp;
using namespace lspgen;

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		std::cerr << "Expected the input file name as the first and only argument" << std::endl;
		return EXIT_FAILURE;
	}

	int ExitCode = EXIT_SUCCESS;
	const char* inputFileName = argv[1];

	if(std::ifstream in{inputFileName, std::ios::binary})
	{
		try
		{
			in.seekg(0, std::ios::end);
			std::streamsize size = in.tellg();
			in.seekg(0, std::ios::beg);
			std::string jsonText;
			jsonText.resize(static_cast<std::string::size_type>(size));
			in.read(&jsonText[0], size);
			in.close();
			auto json = json::parse(jsonText).object();
			MetaModel metaModel;
			metaModel.extract(json);
			CppGenerator generator{&metaModel};
			generator.generate();
			generator.writeFiles();
		}
		catch(const json::ParseError& e)
		{
			std::cerr << "JSON parse error at offset " << e.textPos() << ": " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}
		catch(const std::exception& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
			ExitCode = EXIT_FAILURE;
		}
		catch(...)
		{
			std::cerr << "Unknown error" << std::endl;
			ExitCode = EXIT_FAILURE;
		}
	}
	else
	{
		std::cerr << "Failed to open " << inputFileName << std::endl;
		ExitCode = EXIT_FAILURE;
	}

	return ExitCode;
}
