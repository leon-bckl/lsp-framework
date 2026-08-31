#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <lsp/error.h>
#include <lsp/io/socket.h>
#include <lsp/io/standard_io.h>
#include <lsp/messages.h>
#include <lsp/protocol_version.h>
#include <lsp/server_endpoint.h>

/*
 * This is an example implementation of a simple server using the lsp-framework.
 * It demonstrates how to create a language server that is either
 * 1. listening for incoming client connections on a given port
 *     $ example_server --port=12345
 * 2. started by a client and communicating via stdio
 *
 * The example demonstrates basic text document operations (open, close), as well as
 * hover and diagnostics. It also shows how to create regular and async request handlers.
 *
 * Note that this example is focused on the usage of the lsp-framework.
 * For usage information about the protocol itself, see the official documentation at
 * https://microsoft.github.io/language-server-protocol
 */

namespace{

// Write messages to stderr because stdout is already used for lsp messages
void logStdErr(std::string_view message)
{
	std::cerr << "example_server: " << message << std::endl;
}

// Find TODO messages and report them as diagnostics
auto computeDiagnostics(std::string_view text) -> std::vector<lsp::Diagnostic>
{
	const auto pos = text.find("TODO");

	if(pos == std::string::npos)
		return {};

	const auto line = static_cast<lsp::Uint>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(pos), '\n'));

	return {{
		.range    = {.start = {.line = line, .character = 0}, .end = {.line = line, .character = 4}},
		.message  = "Unresolved TODO",
		.severity = lsp::DiagnosticSeverity::Warning,
		.source   = "example_server",
	}};
}

/*
 * ExampleServer
 */

class ExampleServer{
public:
	explicit ExampleServer(lsp::io::Stream& io)
		: m_endpoint{io}
	{
		registerHandlers();
	}

	// Process messages until the client sends 'exit' or the connection is aborted
	void run()
	{
		m_endpoint.runMessageLoop();
	}

private:
	lsp::ServerEndpoint                               m_endpoint;
	std::mutex                                        m_documentsMutex;
	std::unordered_map<lsp::DocumentUri, std::string> m_documents;

	// Send a window/logMessage to the client and mirror it to the console
	void log(lsp::MessageType type, const std::string& message)
	{
		logStdErr(message);
		m_endpoint.windowLogMessage({.type = type, .message = "example_server: " + message});
	}

	// Verify that a document is actually opened
	void requireOpen(const lsp::DocumentUri& uri)
	{
		if(!m_documents.contains(uri))
			throw lsp::RequestError(lsp::MessageError::InvalidParams, "Unknown document: " + uri.toString());
	}

	void registerHandlers()
	{
		m_endpoint
			.onInitialize(
				[this](auto&& params)
				{
					return initialize(params);
				})
			.onInitialized(
				[this](auto&&)
				{
					log(lsp::MessageType::Info, "client is ready");
				})
			.onTextDocumentDidOpen(
				[this](auto&& params)
				{
					didOpen(std::move(params));
				})
			.onTextDocumentDidClose(
				[this](auto&& params)
				{
					auto lock = std::lock_guard(m_documentsMutex);
					m_documents.erase(params.textDocument.uri);
				})
			.onTextDocumentHover(
				[this](auto&& params)
				{
					return hover(params);
				})
			.onTextDocumentDefinition(
				[this](auto&& params)
				{
					return definitionAsync(std::move(params));
				})
			.onWorkspaceExecuteCommand(
				[this](auto&& params)
				{
					return executeCommand(params);
				})
			.onShutdown(
				[]() -> lsp::ShutdownResult
				{
					// After 'shutdown' the framework rejects any outgoing message,
					// so only local cleanup belongs here
					logStdErr("shutting down");
					return {};
				})
			.onExit(
				[]()
				{
					// The framework has already stopped the message loop by now
					logStdErr("exit");
				});
	}

	/*
	 * Synchronous request handler: the result is returned directly and sent back to the client
	 */
	auto initialize(const lsp::InitializeParams& params) -> lsp::InitializeResult
	{
		const auto clientName = params.clientInfo ? params.clientInfo->name : "unknown client";
		logStdErr("initialize from " + clientName);

		// ServerInfo only became a standalone type in 3.18
		// When built with the older meta model, it uses a generated type name
	#if LSP_PROTOCOL_VERSION < LSP_INT_VERSION(3, 18, 0)
		using ServerInfo = lsp::InitializeResultServerInfo;
	#else
		using ServerInfo = lsp::ServerInfo;
	#endif

		return {
			.capabilities = {
				.positionEncoding  = lsp::PositionEncodingKind::UTF16,
				.textDocumentSync  = lsp::TextDocumentSyncOptions{
					.openClose = true,
					.change    = lsp::TextDocumentSyncKind::Full,
				},
				.hoverProvider      = true,
				.definitionProvider = true,
				.executeCommandProvider = lsp::ExecuteCommandOptions{
					.commands = {"example.showMessage"},
				},
			},
			.serverInfo = ServerInfo{
				.name    = "Example Server",
				.version = "1.0.0",
			},
		};
	}

	void didOpen(lsp::DidOpenTextDocumentParams&& params)
	{
		auto  lock = std::lock_guard(m_documentsMutex);
		auto& doc  = m_documents[params.textDocument.uri];
		doc = std::move(params.textDocument.text);

		// Send a textDocument/publishDiagnostics notification to the client
		m_endpoint.textDocumentPublishDiagnostics({
			.uri         = params.textDocument.uri,
			.diagnostics = computeDiagnostics(doc),
			.version     = params.textDocument.version,
		});
	}

	auto hover(const lsp::HoverParams& params) -> lsp::TextDocumentHoverResult
	{
		{
			auto lock = std::lock_guard(m_documentsMutex);
			requireOpen(params.textDocument.uri);
		}

		return lsp::Hover{
			.contents = lsp::MarkupContent{
				.kind  = lsp::MarkupKind::PlainText,
				.value = "Hover at line " + std::to_string(params.position.line) +
				         ", character " + std::to_string(params.position.character),
			},
		};
	}

	/*
	 * Asynchronous request handler: returning a std::future tells the framework to
	 * finish the work on a worker thread. std::launch::deferred means the lambda body
	 * only runs when the framework calls get() on that thread, so no need to create extra threads here
	 */
	auto definitionAsync(lsp::DefinitionParams&& params) -> std::future<lsp::TextDocumentDefinitionResult>
	{
		return std::async(std::launch::deferred,
			[this, params = std::move(params)]() -> lsp::TextDocumentDefinitionResult
			{
				{
					auto lock = std::lock_guard(m_documentsMutex);
					requireOpen(params.textDocument.uri);
				}

				// Simulate longer running task
				std::this_thread::sleep_for(std::chrono::milliseconds(500));

				const auto line = params.position.line;
				return lsp::Location{
					.uri   = params.textDocument.uri,
					.range = {.start = {.line = line, .character = 0}, .end = {.line = line, .character = 0}},
				};
			});
	}

	auto executeCommand(const lsp::ExecuteCommandParams& params) -> lsp::WorkspaceExecuteCommandResult
	{
		if(params.command != "example.showMessage")
			throw lsp::RequestError(lsp::MessageError::InvalidParams, "Unknown command: " + params.command);

		// the request callbacks are executed on the same thread as the message loop
		m_endpoint.windowShowMessageRequest(
			{
				.type    = lsp::MessageType::Info,
				.message = "example.showMessage was executed. Pick one:",
				.actions = {{{.title = "Thanks"}, {.title = "No thanks"}}},
			},
			[this](lsp::WindowShowMessageRequestResult&& result)
			{
				if(result.isNull())
					log(lsp::MessageType::Info, "message dismissed");
				else
					log(lsp::MessageType::Info, "user picked '" + result->title + '\'');
			},
			[this](const lsp::ResponseError& error)
			{
				log(lsp::MessageType::Error, std::string("showMessageRequest failed: ") + error.what());
			});

		return nullptr; // NullOr<LSPAny> -> null
	}
};

auto parsePortArg(int argc, char** argv) -> std::optional<unsigned short>
{
	constexpr auto PortArg = std::string_view("--port=");

	for(int i = 1; i < argc; ++i)
	{
		const auto arg = std::string_view(argv[i]);

		if(!arg.starts_with(PortArg))
		{
			logStdErr("ignoring unknown argument '" + std::string(arg) + '\'');
			continue;
		}

		const auto portStr = arg.substr(PortArg.size());
		unsigned short port = 0;
		const auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
		(void)ptr;

		if(ec == std::errc{})
			return port;

		logStdErr("invalid port '" + std::string(portStr) + '\'');
	}

	return std::nullopt;
}

auto runStdioServer() -> int
{
	auto server = ExampleServer(lsp::io::standardIO());
	server.run();
	return EXIT_SUCCESS;
}

auto runSocketServer(unsigned short port) -> int
{
	auto listener = lsp::io::SocketListener(port);
	logStdErr("listening for incoming connections on port " + std::to_string(listener.port()));

	while(listener.isOpen())
	{
		auto socket = listener.accept();

		if(!socket.isOpen())
			return EXIT_FAILURE;

		std::thread(
			[socket = std::move(socket)]() mutable
			{
				logStdErr("client connected");

				auto server = ExampleServer(socket);
				server.run();

				logStdErr("client disconnected");
			}).detach();
	}

	return EXIT_SUCCESS;
}

} // namespace

auto main(int argc, char** argv) -> int
{
	try
	{
		if(const auto port = parsePortArg(argc, argv))
			return runSocketServer(*port);

		logStdErr("starting stdio server - pass '--port=<port>' for a socket server");
		return runStdioServer();
	}
	catch(const std::exception& e)
	{
		logStdErr(e.what());
		return EXIT_FAILURE;
	}
}
