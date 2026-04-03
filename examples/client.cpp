#include <atomic>
#include <charconv>
#include <iostream>
#include <thread>
#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <lsp/process.h>
#include <lsp/io/socket.h>
#include <lsp/io/standardio.h>

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
void printResponse(const typename MessageType::Result& result)
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
 * LanguageClient
 */

class LanguageClient{
public:
	LanguageClient(lsp::io::Stream& io)
		: m_connection{io}
		, m_messageHandler{m_connection}
	{
	}

	~LanguageClient()
	{
		if(isRunning())
			shutdown();
	}

	bool isRunning() const
	{
		return m_running.load();
	}

	void startup()
	{
		m_running.store(true);
		m_messageThread = std::thread([this](){ messageLoop(); });

		/*
		 * Setup initialize params with client capabilities
		 */
		auto initializeParams = lsp::requests::Initialize::Params();
		initializeParams.processId    = lsp::Process::currentProcessId(),
		initializeParams.rootUri      = lsp::DocumentUri::fromPath(".");
		initializeParams.capabilities = {
			.textDocument = lsp::TextDocumentClientCapabilities{
				.hover = lsp::HoverClientCapabilities{
					.contentFormat = {{lsp::MarkupKind::PlainText}}
				}
			}
		};

		/*
		 * Send initialize request to the server and wait for the response
		 */
		auto initializeRequest =
			m_messageHandler.sendRequest<lsp::requests::Initialize>(std::move(initializeParams));
		auto initializeResult = initializeRequest.result.get();

		printResponse<lsp::requests::Initialize>(initializeResult);
		m_serverCapabilities = std::move(initializeResult.capabilities);

		/*
		 * Send the 'initialized' notification to let the server know that the client is ready
		 */
		m_messageHandler.sendNotification<lsp::notifications::Initialized>({});
	}

	void shutdown()
	{
		if(!isRunning())
			return;

		/*
		 * Shut down the server by:
		 *   1. Sending a shutdown request
		 *   2. Waiting for the response
		 *   3. Sending an exit notification
		 * The callbacks are executed by the message handling thread
		 */
		m_messageHandler.sendRequest<lsp::requests::Shutdown>(
			[this](const lsp::requests::Shutdown::Result&)
			{
				m_messageHandler.sendNotification<lsp::notifications::Exit>();
				m_running.store(false);
			},
			[](const lsp::ResponseError& error)
			{
				printError(error);
			});

		if(m_messageThread.joinable())
			m_messageThread.join();
	}

	void openDocument(lsp::DocumentUri uri, std::string text, std::string languageId)
	{
		m_messageHandler.sendNotification<lsp::notifications::TextDocument_DidOpen>(
			{
				.textDocument = {
					.uri = std::move(uri),
					.languageId = std::move(languageId),
					.version = 1,
					.text = std::move(text)
				}
			});
	}

	lsp::requests::TextDocument_Hover::Result hover(lsp::DocumentUri uri, const lsp::Position& position)
	{
		try
		{
			auto hoverParams             = lsp::requests::TextDocument_Hover::Params();
			hoverParams.textDocument.uri = std::move(uri);
			hoverParams.position         = position;

			auto hoverRequest =
				m_messageHandler.sendRequest<lsp::requests::TextDocument_Hover>(std::move(hoverParams));

			const auto hoverResult = hoverRequest.result.get();

			printResponse<lsp::requests::TextDocument_Hover>(hoverResult);

			return hoverResult;
		}
		catch(const lsp::ResponseError& error)
		{
			printError(error);
			return {};
		}
	}

private:
	lsp::Connection         m_connection;
	lsp::MessageHandler     m_messageHandler;
	std::atomic_bool        m_running = false;
	std::thread             m_messageThread;
	lsp::ServerCapabilities m_serverCapabilities;

	void messageLoop()
	{
		try
		{
			while(isRunning())
				m_messageHandler.processIncomingMessages();
		}
		catch(const std::exception& e)
		{
			m_running.store(false);
			std::cerr << "ERROR: " << e.what() << std::endl;
		}
	}
};

/*
 * Run a language client with the given io
 */

void runLanguageClient(lsp::io::Stream& io)
{
	auto client = LanguageClient(io);
	client.startup();

	// Open a document and send a hover request

	const auto documentUri = lsp::DocumentUri::fromPath("foo.txt");
	client.openDocument(documentUri, "bar", "txt");
	const auto hoverResult = client.hover(documentUri, {1, 1});

	if(hoverResult.isNull())
	{
		std::cerr << "No hover available" << std::endl;
	}
	else
	{
		if(const auto* contents = std::get_if<lsp::MarkupContent>(&hoverResult->contents))
		{
			std::cerr << "Hover contents: "
				<< lsp::MarkupKindEnum::value(contents->kind)
				<< ", "
				<< contents->value
				<< std::endl;
		}
		else
		{
			std::cerr << "Unsupported hover content" << std::endl;
		}
	}

	client.shutdown();
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
			// Executable arg was found so add all remaning args to the command line
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
	try
	{
		const auto args = parseArgs(argc, argv);

		if(!args.executable.empty())
		{
			std::cerr << "Launching language server executable '" << args.executable << '\'' << std::endl;
			auto proc = lsp::Process(args.executable, args.executableArgs);
			runLanguageClient(proc.stdIO());
		}
		else if(args.port.has_value())
		{
			std::cerr << "Connecting to language server on port " << *args.port << std::endl;
			auto socket = lsp::io::Socket::connect(lsp::io::Socket::Localhost, *args.port);
			runLanguageClient(socket);
		}
		else
		{
			std::cerr << R"(Available arguments:
    --port=<portnum>          Connect to a language server via socket on port <portnum>
    --exe=<executable> <args> Launch language server <executable> and connect to it via stdio)" << std::endl;
			return EXIT_FAILURE;
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
