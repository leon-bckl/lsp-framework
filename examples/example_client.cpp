#include <charconv>
#include <iostream>
#include <thread>
#include <lsp/client_endpoint.h>
#include <lsp/error.h>
#include <lsp/io/socket.h>
#include <lsp/messages.h>
#include <lsp/process.h>
#include <lsp/protocol_version.h>

/*
 * This is an example implementation of a simple client using the lsp-framework.
 * It demonstrates how to create a client that communicates with a language server by either
 * 1. starting the server process and stdio
 *     $ example_client --exe=example_server
 * 2. connecting to an existing server instance via socket
 *     $ example_client --port=12345
 *
 * Once started, the client sends the following requests and notifications to the server:
 * 1. initialize              - Initializes the server
 * 2. initialized             - Notifies the server that the client is ready
 * 3. textDocument/didOpen    - Notifies the server that a text document was opened
 * 4. textDocument/hover      - Requests hover information for a given position in a text document
 * 5. textDocument/definition - Requests the definition of a symbol at a given position
 * 6. textDocument/didClose   - Notifies the server that a text document was closed
 * 7. shutdown                - Tells the server to shut down
 * 8. exit                    - Asks the server to exit its process
 *
 * Note that this example is focused on the usage of the lsp-framework.
 * For usage information about the protocol itself, see the official documentation at
 * https://microsoft.github.io/language-server-protocol
 */

namespace{

// Write messages to stderr because stdout is already used for lsp messages
void logStdErr(std::string_view message)
{
	std::cerr << "example_client: " << message << std::endl;
}

auto messageTypeName(lsp::MessageType type) -> std::string
{
	switch(type)
	{
	case lsp::MessageType::Error:   return "error";
	case lsp::MessageType::Warning: return "warning";
	case lsp::MessageType::Info:    return "info";
	case lsp::MessageType::Log:     return "log";
	case lsp::MessageType::Debug:   return "debug";
	default:                        return "message";
	}
}

void printServerMessage(lsp::MessageType type, const std::string& message)
{
	logStdErr("server message [" + messageTypeName(type) + "] " + message);
}

/*
 * ExampleClient
 */

class ExampleClient{
public:
	explicit ExampleClient(lsp::io::Stream& io)
		: m_endpoint(io)
	{
		registerHandlers();
	}

	~ExampleClient()
	{
		stop();
	}

	// Start the background message loop and send an initialize request to the server
	void start()
	{
		if(!m_stopped.exchange(false))
			return;

		m_messageThread = std::thread([this]{ m_endpoint.runMessageLoop(); });

		auto params = lsp::InitializeParams();
		params.processId = lsp::Process::currentProcessId();
		params.rootUri   = lsp::Uri::fileUriFromPath(".");

		// ClientInfo only became a standalone type in 3.18
		// When built with the older meta model, it uses a generated type name
	#if LSP_PROTOCOL_VERSION < LSP_INT_VERSION(3, 18, 0)
		using ClientInfo = lsp::_InitializeParamsClientInfo;
	#else
		using ClientInfo = lsp::ClientInfo;
	#endif

		params.clientInfo   = ClientInfo{.name = "Example Client", .version = "1.0.0"};
		params.capabilities = {
			.textDocument = lsp::TextDocumentClientCapabilities{
				.hover = lsp::HoverClientCapabilities{
					.contentFormat = {{lsp::MarkupKind::PlainText, lsp::MarkupKind::Markdown}},
				},
			},
		};

		// initialize() returns a RequestResult. get() blocks until the response becomes available
		const auto result = m_endpoint.initialize(params).get();
		logStdErr("connected to " + (result.serverInfo ? result.serverInfo->name : std::string("unknown server")));

		// Notify the server that the client is ready
		m_endpoint.initialized({});
	}

	void openDocument(const lsp::DocumentUri& uri, std::string languageId, std::string text)
	{
		m_endpoint.textDocumentDidOpen({
			.textDocument = {
				.uri        = uri,
				.languageId = std::move(languageId),
				.version    = 1,
				.text       = std::move(text),
			},
		});
	}

	void closeDocument(const lsp::DocumentUri& uri)
	{
		m_endpoint.textDocumentDidClose({.textDocument = {.uri = uri}});
	}

	void requestHover(const lsp::DocumentUri& uri, lsp::Position position)
	{
		auto params = lsp::HoverParams();
		params.textDocument.uri = uri;
		params.position         = position;

		try
		{
			const auto result = m_endpoint.textDocumentHover(params).get();

			if(result.isNull())
				logStdErr("hover -> (none)");
			else if(const auto* markup = std::get_if<lsp::MarkupContent>(&result->contents))
				logStdErr("hover -> " + markup->value);
			else
				logStdErr("hover -> (unsupported content kind)");
		}
		catch(const lsp::ResponseError& e)
		{
			logStdErr(std::string("hover failed: ") + e.what());
		}
	}

	void requestDefinition(const lsp::DocumentUri& uri, lsp::Position position)
	{
		auto params = lsp::DefinitionParams();
		params.textDocument.uri = uri;
		params.position         = position;

		try
		{
			const auto result = m_endpoint.textDocumentDefinition(params).get();

			if(result.isNull())
			{
				logStdErr("definition -> (none)");
				return;
			}

			const auto* definition = std::get_if<lsp::Definition>(&result.value());
			const auto* location   = definition ? std::get_if<lsp::Location>(definition) : nullptr;

			if(location)
				logStdErr("definition -> " + location->uri.toString() + " line " + std::to_string(location->range.start.line));
			else
				logStdErr("definition -> (unsupported result shape)");
		}
		catch(const lsp::ResponseError& e)
		{
			logStdErr(std::string("definition failed: ") + e.what());
		}
	}

	void runCommand(std::string command)
	{
		auto params = lsp::ExecuteCommandParams();
		params.command = std::move(command);

		try
		{
			(void)m_endpoint.workspaceExecuteCommand(params).get();
			logStdErr("command '" + params.command + "' done");
		}
		catch(const lsp::ResponseError& e)
		{
			logStdErr(std::string("command failed: ") + e.what());
		}
	}

	 // Shut the server down and stop the message loop
	void stop()
	{
		if(m_stopped.exchange(true))
			return;

		// Send a shutdown request followed by an exit notification as required by the protocol
		m_endpoint.shutdown(
			[this](auto&&)
			{
				m_endpoint.exit();
			},
			[](const lsp::ResponseError& e)
			{
				logStdErr(std::string("shutdown failed: ") + e.what());
			});

		if(m_messageThread.joinable())
			m_messageThread.join();
	}

private:
	lsp::ClientEndpoint m_endpoint;
	std::thread         m_messageThread;
	std::atomic_bool    m_stopped = true;

	void registerHandlers()
	{
		m_endpoint
			.onWindowLogMessage(
				[](auto&& params)
				{
					printServerMessage(params.type, params.message);
				})
			.onWindowShowMessage(
				[](auto&& params)
				{
					printServerMessage(params.type, params.message);
				})
			.onTextDocumentPublishDiagnostics(
				[](lsp::notifications::TextDocumentPublishDiagnostics::Params&& params)
				{
					logStdErr(std::to_string(params.diagnostics.size()) + " diagnostic(s) for " + params.uri.toString());

					for(const auto& diagnostic : params.diagnostics)
					{
						// Diagnostic::message is a plain string in 3.17 and can be MarkupContent in 3.18
					#if LSP_PROTOCOL_VERSION < LSP_INT_VERSION(3, 18, 0)
						logStdErr("  - " + diagnostic.message);
					#else
						const auto* text = std::get_if<lsp::String>(&diagnostic.message);
						logStdErr("  - " + (text ? *text : std::get<lsp::MarkupContent>(diagnostic.message).value));
					#endif
					}
				})
			.onWindowShowMessageRequest(
				[](auto&& params) -> lsp::WindowShowMessageRequestResult
				{
					logStdErr("server asks: " + params.message);

					// Just pick the first action
					if(params.actions && !params.actions->empty())
					{
						const auto& choice = params.actions->front();
						logStdErr("answering with '" + choice.title + '\'');
						return choice;
					}

					return nullptr;
				});
	}
};

struct Args{
	std::optional<unsigned short> port;
	std::string                   executable;
	std::vector<std::string>      executableArgs;
};

auto parseArgs(int argc, char** argv) -> Args
{
	constexpr auto PortArg = std::string_view("--port=");
	constexpr auto ExeArg  = std::string_view("--exe=");

	auto args = Args();

	for(int i = 1; i < argc; ++i)
	{
		const auto arg = std::string_view(argv[i]);

		if(!args.executable.empty())
		{
			args.executableArgs.emplace_back(arg); // everything after --exe= goes to the server
		}
		else if(arg.starts_with(ExeArg))
		{
			args.executable = arg.substr(ExeArg.size());
		}
		else if(arg.starts_with(PortArg))
		{
			const auto     portStr   = arg.substr(PortArg.size());
			unsigned short port      = 0;
			const auto     [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
			(void)ptr;

			if(ec == std::errc{})
				args.port = port;
			else
				logStdErr("invalid port '" + std::string(portStr) + '\'');
		}
		else
		{
			logStdErr("ignoring unknown argument '" + std::string(arg) + '\'');
		}
	}

	return args;
}

void runClientSession(lsp::io::Stream& io)
{
	auto client = ExampleClient(io);
	client.start();

	const auto uri = lsp::Uri::fileUriFromPath("example.txt");
	client.openDocument(uri, "plaintext", "First line\nTODO: write the rest\n");
	client.requestHover(uri, {0, 3});
	client.requestDefinition(uri, {1, 0});
	client.runCommand("example.showMessage");
	client.closeDocument(uri);

	client.stop();

	logStdErr("exiting...");
}

} // namespace

auto main(int argc, char** argv) -> int
{
	const auto args = parseArgs(argc, argv);

	try
	{
		if(!args.executable.empty())
		{
			logStdErr("launching server '" + args.executable + '\'');
			auto process = lsp::Process::start(args.executable, args.executableArgs);
			runClientSession(process.stdIO());
		}
		else if(args.port)
		{
			logStdErr("connecting to port " + std::to_string(*args.port));
			auto socket = lsp::io::Socket::connect(lsp::io::Socket::Localhost, *args.port);
			runClientSession(socket);
		}
		else
		{
			logStdErr("usage: example_client --exe=<server> [server args]  |  example_client --port=<port>");
			return EXIT_FAILURE;
		}
	}
	catch(const std::exception& e)
	{
		logStdErr(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
