#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <lsp/connection.h>
#include <lsp/io/socket.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <lsp/process.h>

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

class LanguageServer{
public:
	LanguageServer(lsp::io::Stream& io)
		: m_connection{io}
		, m_messageHandler{m_connection}
	{
		registerCallbacks();
		m_state.store(State::Uninitialized);
	}

	bool isRunning()
	{
		return m_state.load() != State::Inactive &&
		       (m_parentProcessId.isNull() || lsp::Process::exists(*m_parentProcessId));
	}

	int run()
	{
		try
		{
			while(isRunning())
				m_messageHandler.processIncomingMessages();
		}
		catch(const std::exception& e)
		{
			std::cerr << "ERROR: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	auto initialize(lsp::InitializeParams&& params) -> lsp::requests::Initialize::Result
	{
		std::cerr << "INITIALIZE" << std::endl;
		printMessage<lsp::requests::Initialize>(params);

		if(m_state > State::Uninitialized)
			throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Already initialized");

		m_state.store(State::Active);
		m_parentProcessId = params.processId;

		/*
		 * Respond with an InitializeResult containing some basic server info and capabilities
		 */

		return {
			.capabilities = {
				.positionEncoding = lsp::PositionEncodingKind::UTF16,
				.textDocumentSync = lsp::TextDocumentSyncOptions{
					.openClose = true,
					.change    = lsp::TextDocumentSyncKind::Full,
					.save      = true
				},
				.hoverProvider = true,
			},
			.serverInfo = lsp::InitializeResultServerInfo{
				.name    = "Language Server Example",
				.version = "1.0.0"
			},
		};
	}

	void textDocumentDidOpen(lsp::notifications::TextDocument_DidOpen::Params&&)
	{
		verifyInitialized();
		// Do something with the openend document here...
	}

	auto hover(lsp::requests::TextDocument_Hover::Params&& params) -> lsp::AsyncRequestResult<lsp::requests::TextDocument_Hover>
	{
		printMessage<lsp::requests::TextDocument_Hover>(params);
		verifyInitialized();

		// Verify that the server actually knows the document from the request
		if(params.textDocument.uri != lsp::DocumentUri::fromPath("foo.txt"))
		{
			throw lsp::RequestError(lsp::MessageError::InvalidParams,
			                        "Unknown document: " + params.textDocument.uri.toString());
		}

		/*
		 * Handle the request asynchronously.
		 * It is executed in a worker thread by the message handler.
		 * This means a deferred future can be used and it is not necessary to spawn extra threads.
		 */
		return std::async(std::launch::deferred,
			[params = std::move(params)]()
			{
				// simulate longer running task
				std::this_thread::sleep_for(std::chrono::seconds(2));

				// return the result
				// TextDocument_Hover::Result is NullOr<Hover>
				auto hover = lsp::Hover{
					.contents = lsp::MarkupContent{
						.kind  = lsp::MarkupKind::PlainText,
						.value = "Hover test"
					}
				};
				return lsp::requests::TextDocument_Hover::Result(std::move(hover));
			}
		);
	}

	auto shutdown() -> lsp::requests::Shutdown::Result
	{
		printMessage<lsp::requests::Shutdown>();
		m_state.store(State::Shutdown);
		return {};
	}

	void exit()
	{
		printMessage<lsp::notifications::Exit>();
		m_state.store(State::Inactive);
	}

private:
	lsp::Connection     m_connection;
	lsp::MessageHandler m_messageHandler;
	lsp::NullOr<int>    m_parentProcessId;

	enum class State{
		Inactive,      // Initial state or exit notification received
		Uninitialized, // Started up and waiting for initialize request
		Active,        // Currently handling requests
		Shutdown       // Shutdown notification received
	};

	std::atomic<State> m_state = State::Uninitialized;

	void verifyInitialized() const
	{
		if(m_state.load() <= State::Uninitialized)
			throw lsp::RequestError(lsp::MessageError::ServerNotInitialized, "Server not initialized");

		if(m_state.load() == State::Shutdown)
			throw lsp::RequestError(lsp::MessageError::InvalidRequest, "Shutdown request received");
	}

	void registerCallbacks()
	{
		m_messageHandler.add<lsp::requests::Initialize>(
			[this](lsp::requests::Initialize::Params&& params)
			{
				return initialize(std::move(params));
			}
		).add<lsp::notifications::TextDocument_DidOpen>(
			[this](lsp::notifications::TextDocument_DidOpen::Params&& params)
			{
				textDocumentDidOpen(std::move(params));
			}
		).add<lsp::requests::TextDocument_Hover>(
			[this](lsp::requests::TextDocument_Hover::Params&& params)
			{
				return hover(std::move(params));
			}
		).add<lsp::requests::Shutdown>(
			[this]()
			{
				return shutdown();
			}
		).add<lsp::notifications::Exit>(
			[this]()
			{
				exit();
			}
		);
	}
};

/*
 * Socket server
 */

int runSocketServer(unsigned short port)
{
	try
	{
		std::cerr << "Waiting for incoming connections..." << std::endl;

		auto socketListener = lsp::io::SocketListener(port);

		while(socketListener.isReady())
		{
			auto socket = socketListener.listen();

			if(!socket.isOpen())
				break;

			std::cerr << "Accepted connection" << std::endl;

			auto thread = std::thread(
				[socket = std::move(socket)]() mutable
				{
					auto server = LanguageServer(socket);
					server.run();
				}
			);
			thread.detach();
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/*
 * stdio server
 */

int runStdioServer()
{
	auto server = LanguageServer(lsp::io::standardIO());
	return server.run();
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
	const auto port = parsePortArg(argc, argv);

	if(!port.has_value())
	{
		std::cerr << "Starting stdio server - Launch with '--port=<portnum>' to run a socket server" << std::endl;
		return runStdioServer();
	}

	std::cerr << "Starting socket server on port " << *port << std::endl;
	return runSocketServer(*port);
}
