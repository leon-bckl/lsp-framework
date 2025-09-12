#include <charconv>
#include <iostream>
#include <lsp/connection.h>
#include <lsp/io/socket.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <lsp/process.h>
#include <thread>

/*
 * This is an example implementation of a simple client using the lsp-framework.
 * It demonstrates how to create a client that communicates with a language server by either
 * 1. starting the server process and stdio
 *     $ LspClientExample --exe=LspServerExample
 * 2. connecting to an existing server instance via sockets
 *     $ LspClientExample --port=12345
 *
 * Once started, the client sends the following requests and notifications to the server:
 * 1. initialize          - Initializes the server
 * 2. initialized         - Notifies the server that the client is ready
 * 3. textDocument/hover  - Requests hover information for a given position in a text document
 * 4. shutdown            - Tells the server to shut down
 * 5. exit                - Asks the server to exit its process
 *
 * Incoming messages are written to stderr.
 *
 * Note that this example is focused on the usage of the lsp-framework.
 * For usage information about the protocol itself, see the official documentation at
 * https://microsoft.github.io/language-server-protocol
 */

namespace{

/*
 * Thread that auto joins on destruction because std::jthread is not available on Apple clang
 */
class JThread{
public:
	JThread(std::thread&& thread) : m_thread{std::move(thread)}{}
	~JThread(){ m_thread.join(); }
private:
	 std::thread m_thread;
};

/*
 * Helpers to print the message method and payload
 */

template<typename MessageType>
void printMessageMethod()
{
	std::cerr << "Client received response for '" << MessageType::Method << '\'' << std::endl;
}

template<typename MessageType>
void printMessagePayload(const typename MessageType::Result& result)
{
	const auto json = lsp::toJson(typename MessageType::Result(result));
	std::cerr << "payload: " << lsp::json::stringify(json, true) << std::endl;
}

template<typename MessageType>
void printMessage(const typename MessageType::Result& result)
{
	printMessageMethod<MessageType>();
	printMessagePayload<MessageType>(result);
}

template<typename MessageType>
void printMessage()
{
	printMessageMethod<MessageType>();
}

void printError(const lsp::ResponseError& error)
{
	std::cerr << "ERROR: " << error.code() << " - " << error.what() << std::endl;
}

/*
 * Message processing
 */

bool g_running = false; // This is only changed inside of the shutdown callback on the message processing thread

JThread startMessageProcessingThread(lsp::MessageHandler& messageHandler)
{
	return std::thread([&messageHandler]()
	{
		g_running = true;
		while(g_running)
			messageHandler.processIncomingMessages();
	});
}

/*
 * Actual client implementation
 */

void runLanguageClient(lsp::io::Stream& io)
{
	auto connection     = lsp::Connection(io);
	auto messageHandler = lsp::MessageHandler(connection);
	auto thread         = startMessageProcessingThread(messageHandler);

	/*
	 * Send initialize request to the server and wait for the response
	 */
	auto initializeParams = lsp::requests::Initialize::Params();
	initializeParams.rootUri = lsp::DocumentUri::fromPath(".");
	initializeParams.capabilities = {
		.textDocument = lsp::TextDocumentClientCapabilities{
			.hover = lsp::HoverClientCapabilities{
				.contentFormat = {{lsp::MarkupKind::PlainText}}
			}
		}
	};
	auto initializeRequest =
		messageHandler.sendRequest<lsp::requests::Initialize>(std::move(initializeParams));
	const auto initializeResult = initializeRequest.result.get();

	printMessage<lsp::requests::Initialize>(initializeResult);

	/*
	 * Send the 'initialized' notification to let the server know that the client is ready
	 */
	messageHandler.sendNotification<lsp::notifications::Initialized>(lsp::notifications::Initialized::Params{});

	/*
	 * Send a hover request to the server if it has the capabilitiy.
	 * This check would not be sufficient to verify that the server supports hover requests
	 * in a real client implementation because hoverProvider can be a bool or lsp::HoverOptions.
	 */
	if(initializeResult.capabilities.hoverProvider.has_value())
	{
		try
		{
			auto hoverParams             = lsp::requests::TextDocument_Hover::Params();
			hoverParams.textDocument.uri = lsp::DocumentUri::fromPath("some_file.txt");
			hoverParams.position         = {.line = 2, .character = 5};

			auto hoverRequest =
				messageHandler.sendRequest<lsp::requests::TextDocument_Hover>(std::move(hoverParams));

			const auto hoverResult = hoverRequest.result.get();
			printMessage<lsp::requests::TextDocument_Hover>(hoverResult);
		}
		catch(const lsp::ResponseError& error)
		{
			printError(error);
		}
	}

	/*
	 * Shut down the server by:
	 *   1. Sending a shutdown request
	 *   2. Waiting for the response
	 *   3. Sending an exit notification
	 * The callbacks are executed by the message handling thread
	 */
	messageHandler.sendRequest<lsp::requests::Shutdown>(
		[&messageHandler](lsp::requests::Shutdown::Result&& result)
		{
			printMessage<lsp::requests::Shutdown>(result);
			messageHandler.sendNotification<lsp::notifications::Exit>();
			g_running = false;
		},
		[](const lsp::ResponseError& error)
		{
			printError(error);
			g_running = false;
		});
}

/*
 * Argument parsing
 */

struct Args{
	std::optional<unsigned short> port;
	std::string                   executable;
	std::vector<std::string>      executableArgs;
};

Args parseArgs(int argc, char** argv)
{
	constexpr auto PortArg = std::string_view("--port=");
	constexpr auto ExeArg  = std::string_view("--exe=");

	auto args = Args();

	for(int i = 1; i < argc; ++i)
	{
		const auto arg = std::string_view(argv[i]);

		if(!args.executable.empty())
		{
			// Executable arg was found so add all remaning args are the command line
			args.executableArgs.push_back(std::string(arg));
		}
		else if(arg.starts_with(PortArg))
		{
			unsigned short port;
			const auto portStr = arg.substr(PortArg.size());
			const auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
			(void)ptr;

			if(ec == std::errc{})
				args.port = port;
			else
				std::cerr << "Invalid port: " << portStr << std::endl;
		}
		else if(arg.starts_with(ExeArg))
		{
			const auto executable = arg.substr(ExeArg.size());

			if(!executable.empty())
				args.executable = executable;
		}
		else
		{
			std::cerr << "Unknown argument: " << arg << std::endl;
		}
	}

	return args;
}

} // namespace

int main(int argc, char** argv)
{
	const auto args = parseArgs(argc, argv);

	if(!args.port.has_value() && args.executable.empty())
	{
		std::cerr << R"(Available arguments:
    --port=<portnum>          Connect to a language server via socket on port <portnum>
    --exe=<executable> <args> Launch language server <executable> and connect to it via stdio)" << std::endl;
		return 1;
	}

	try
	{
		if(args.port.has_value())
		{
			std::cerr << "Connecting to language server on port " << *args.port << std::endl;
			auto socket = lsp::io::Socket::connect(lsp::io::Socket::Localhost, *args.port);
			runLanguageClient(socket);
		}
		else
		{
			std::cerr << "Launching language server executable '" << args.executable << '\'' << std::endl;
			auto proc = lsp::Process(args.executable, args.executableArgs);
			runLanguageClient(proc.stdIO());
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
