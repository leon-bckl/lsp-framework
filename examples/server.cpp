#include <charconv>
#include <chrono>
#include <iostream>
#include <lsp/connection.h>
#include <lsp/io/socket.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <string_view>
#include <thread>

/*
 * This is an example implementation of a simple server using the lsp-framework.
 * It demonstrates how to create a language server that is either
 * 1. listening for incoming client connections on a given port
 *     $ LspServerExample --port=12345
 * 2. started by a client and communicating via stdio
 *
 * Initialization, shutdown and the textDocument/hover request are handled by this example.
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
	const auto type = lsp::message::IsNotification<MessageType> ? "notification" : "request";
	std::cerr << "Server received " << type << " '" << MessageType::Method << '\'' << std::endl;
}

template<typename MessageType>
void printMessagePayload(const typename MessageType::Params& params)
{
	const auto json = lsp::toJson(typename MessageType::Params(params));
	std::cerr << "payload: " << lsp::json::stringify(json, true) << std::endl;
}

template<typename MessageType>
void printMessage(const typename MessageType::Params& params)
{
	printMessageMethod<MessageType>();
	printMessagePayload<MessageType>(params);
}

template<typename MessageType>
void printMessage()
{
	printMessageMethod<MessageType>();
}

/*
 * Callback registration
 */

thread_local bool g_running = false;

void registerCallbacks(lsp::MessageHandler& messageHandler)
{
	messageHandler.add<lsp::requests::Initialize>([](lsp::requests::Initialize::Params&& params)
	{
		printMessage<lsp::requests::Initialize>(params);

		/*
		 * Respond with an InitializeResult containing some basic server info and capabilities
		 */

		return lsp::requests::Initialize::Result{
			.capabilities = {
				.positionEncoding = lsp::PositionEncodingKind::UTF16,
				.textDocumentSync = lsp::TextDocumentSyncOptions{
					.openClose = true,
					.change    = lsp::TextDocumentSyncKind::Full,
					.save      = true
				},
				.hoverProvider      = true,
				.diagnosticProvider = lsp::DiagnosticOptions{
					.interFileDependencies = true
				}
			},
			.serverInfo = lsp::InitializeResultServerInfo{
				.name    = "Language Server Example",
				.version = "1.0.0"
			},
		};
	})
	.add<lsp::requests::TextDocument_Hover>([](lsp::requests::TextDocument_Hover::Params&& params)
	{
		printMessage<lsp::requests::TextDocument_Hover>(params);

		/*
		 * Handle the request asynchronously.
		 * It is executed in a worker thread by the message handler.
		 * This means a deferred future can be used and it is not necessary to spawn extra threads.
		 */
		return std::async(std::launch::deferred, [params = std::move(params)]()
		{
			// simulate longer running task
			std::this_thread::sleep_for(std::chrono::seconds(2));

			// return the result
			// TextDocument_Hover::Result is NullOr<Hover>
			auto hover = lsp::Hover{
				.contents = "Hover result"
			};
			return lsp::requests::TextDocument_Hover::Result(std::move(hover));
		});
	})
	.add<lsp::requests::Shutdown>([]()
	{
		printMessage<lsp::requests::Shutdown>();
		return lsp::requests::Shutdown::Result();
	})
	.add<lsp::notifications::Exit>([]()
	{
		printMessage<lsp::notifications::Exit>();
		g_running = false;
	});
}

/*
 * Message processing loop
 */

void runLanguageServer(lsp::io::Stream& io)
{
	auto connection     = lsp::Connection(io);
	auto messageHandler = lsp::MessageHandler(connection);
	registerCallbacks(messageHandler);

	g_running = true;

	while(g_running)
		messageHandler.processIncomingMessages();
}

/*
 * Socket server
 */

void runSocketServer(unsigned short port)
{
	std::cerr << "Waiting for incoming connections..." << std::endl;

	auto socketListener = lsp::io::SocketListener(port);

	while(socketListener.isReady())
	{
		auto socket = socketListener.listen();

		if(!socket.isOpen())
			break;

		std::cerr << "Accepted connection" << std::endl;

		auto thread = std::thread([socket = std::move(socket)]() mutable
		{
			try
			{
				runLanguageServer(socket);
			}
			catch(const std::exception& e)
			{
				std::cerr << "ERROR: " << e.what() << std::endl;
			}
		});
		thread.detach();
	}
}

/*
 * stdio server
 */

void runStdioServer()
{
	runLanguageServer(lsp::io::standardIO());
}

/*
 * Argument parsing
 */

std::optional<unsigned short> parsePortArg(int argc, char** argv)
{
	constexpr auto PortArg = std::string_view("--port=");

	for(int i = 1; i < argc; ++i)
	{
		const auto arg = std::string_view(argv[i]);

		if(arg.starts_with(PortArg))
		{
			unsigned short port;
			const auto portStr = arg.substr(PortArg.size());
			const auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);
			(void)ptr;

			if(ec == std::errc{})
				return port;
		}
		else
		{
			std::cerr << "Unknown argument: " << arg << std::endl;
		}
	}

	return std::nullopt;
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		const auto port = parsePortArg(argc, argv);

		if(!port.has_value())
		{
			std::cerr << "Starting stdio server - Launch with '--port=<portnum>' to run a socket server" << std::endl;
			runStdioServer();
		}
		else
		{
			std::cerr << "Starting socket server on port " << *port << std::endl;
			runSocketServer(*port);
		}

		std::cerr << "Exiting" << std::endl;
	}
	catch(const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
