#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <test/test.h>
#include <lsp/io/socket.h>

using namespace lsp;
using namespace lsp::io;

namespace{

Socket connectWithRetry(const std::string& address, unsigned short port)
{
	constexpr auto MaxAttempts = 100;

	for(int attempt = 0; attempt < MaxAttempts; ++attempt)
	{
		try
		{
			return Socket::connect(address, port);
		}
		catch(const Error&)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	test::fail("Failed to connect after retrying");
}

struct ConnectedPair
{
	Socket client;
	Socket server;
};

ConnectedPair connectPair(SocketListener& listener)
{
	auto serverFuture = std::async(std::launch::async, [&listener](){ return listener.accept(); });
	auto client       = connectWithRetry(Socket::Localhost, listener.port());
	return {std::move(client), serverFuture.get()};
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("ListenerPortZeroAssignsRealPort", [](){
		auto listener = SocketListener(0);
		test::check(listener.port() != 0, "portAssigned");
	});

	app.addTest("ConnectAndExchangeData", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		test::check(client.isOpen(), "clientOpen");
		test::check(server.isOpen(), "serverOpen");

		const auto message = std::string("hello");
		client.write(message.data(), message.size());

		auto received = std::string(message.size(), '\0');
		server.read(received.data(), received.size());
		test::compare(received, message);

		const auto reply = std::string("world");
		server.write(reply.data(), reply.size());

		auto receivedReply = std::string(reply.size(), '\0');
		client.read(receivedReply.data(), receivedReply.size());
		test::compare(receivedReply, reply);
	});

	app.addTest("AcceptedSocketPortMatchesListenerPort", [](){
		auto listener = SocketListener(0);
		const auto port = listener.port();
		auto [client, server] = connectPair(listener);

		test::compare(server.port(), port);
	});

	app.addTest("ClientSocketHasNonZeroPort", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		test::check(client.port() != 0, "clientPortAssigned");
	});

	app.addTest("CloseMakesSocketNotOpen", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		test::check(client.isOpen(), "openBeforeClose");
		client.close();
		test::check(!client.isOpen(), "closedAfterClose");
	});

	app.addTest("SequentialConnectionsToSameListener", [](){
		auto listener = SocketListener(0, 4);

		for(int i = 0; i < 3; ++i)
		{
			auto [client, server] = connectPair(listener);

			test::check(client.isOpen(), "clientOpen");
			test::check(server.isOpen(), "serverOpen");
		}
	});

	app.addTest("ListenerReadyUntilShutdown", [](){
		auto listener = SocketListener(0);
		test::check(listener.isOpen(), "readyAfterConstruction");

		listener.close();
		test::check(!listener.isOpen(), "notReadyAfterShutdown");
	});

	app.addTest("ConnectPortZeroThrows", [](){
		test::expectException<Error>([](){
			(void)Socket::connect(Socket::Localhost, 0);
		}, "Cannot connect on port 0");
	});

	app.addTest("ConnectToClosedPortThrows", [](){
		auto listener = SocketListener(0);
		const auto port = listener.port();
		listener.close(); // frees the port without anyone listening on it

		test::expectException<Error>([port](){
			(void)Socket::connect(Socket::Localhost, port);
		});
	});

	app.addTest("ListenAfterShutdownThrows", [](){
		auto listener = SocketListener(0);
		listener.close();

		test::expectException<Error>([&listener](){
			(void)listener.accept();
		}, "Server socket is not open for listening");
	});

	app.addTest("ReadAfterPeerClosedThrows", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		server.close();

		char buffer[16];
		test::expectException<Error>([&client, &buffer](){
			client.read(buffer, sizeof(buffer));
		});
	});

	app.addTest("MoveConstructTransfersOwnership", [](){
		auto listener = SocketListener(0);
		auto [original, server] = connectPair(listener);

		auto moved = std::move(original);
		test::check(moved.isOpen(), "movedToOpen");
		test::check(!original.isOpen(), "movedFromClosed");

		const auto message = std::string("moved");
		moved.write(message.data(), message.size());

		auto received = std::string(message.size(), '\0');
		server.read(received.data(), received.size());
		test::compare(received, message);
	});

	app.addTest("MoveAssignReplacesExistingConnection", [](){
		auto listener = SocketListener(0, 2);
		auto [clientA, serverA] = connectPair(listener);
		auto [clientB, serverB] = connectPair(listener);

		clientA = std::move(clientB);

		test::check(clientA.isOpen(), "clientAOpenAfterAssign");
		test::check(!clientB.isOpen(), "clientBClosedAfterMove");

		const auto message = std::string("viaB");
		clientA.write(message.data(), message.size());

		auto received = std::string(message.size(), '\0');
		serverB.read(received.data(), received.size());
		test::compare(received, message);
	});

	app.addTest("StreamInterfaceReadWrite", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		Stream& clientStream = client.stream();
		Stream& serverStream = server.stream();

		const auto message = std::string("viaStream");
		clientStream.write(message.data(), message.size());

		auto received = std::string(message.size(), '\0');
		serverStream.read(received.data(), received.size());
		test::compare(received, message);

		Stream& implicitClientStream = client;
		test::check(&implicitClientStream == &clientStream, "implicitConversionSameStream");
	});

	app.addTest("LargePayloadTransfer", [](){
		auto listener = SocketListener(0);
		auto [client, server] = connectPair(listener);

		constexpr auto PayloadSize = std::size_t(1024 * 1024);

		auto payload = std::string(PayloadSize, '\0');
		for(std::size_t i = 0; i < PayloadSize; ++i)
			payload[i] = static_cast<char>(i % 256);

		auto writerFuture = std::async(std::launch::async, [&client, &payload](){ client.write(payload.data(), payload.size()); });
		auto received = std::string(PayloadSize, '\0');
		server.read(received.data(), received.size());

		writerFuture.get();
		test::compare(received, payload);
	});

	return app.main(argc, argv);
}
