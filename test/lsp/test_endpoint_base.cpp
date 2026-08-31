#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <test/test.h>
#include <lsp/connection.h>
#include <lsp/endpoint_base.h>
#include <lsp/error.h>
#include <lsp/io/stream.h>
#include <lsp/message_base.h>
#include <lsp/message_handler.h>

using namespace lsp;

namespace{

struct DummyMessage{};

class LoopbackStream : public io::Stream{
public:
	std::function<void()> onRead;
	int                   readCount = 0;

	void read(char* buffer, std::size_t size) override
	{
		++readCount;

		if(onRead)
			onRead();

		if(m_buffer.size() < size)
			throw io::Error("LoopbackStream is empty");

		std::memcpy(buffer, m_buffer.data(), size);
		m_buffer.erase(0, size);
	}

	void write(const char* buffer, std::size_t size) override
	{
		m_buffer.append(buffer, size);
	}

private:
	std::string m_buffer;
};

// Mirrors the internal state of the endpoint which is not accessible from outside
enum class State{ Uninitialized, Active, Shutdown, Inactive };

void gotoState(ServerEndpointBase& server, State state)
{
	if(state == State::Uninitialized)
		return;

	server.preMethodCall<requests::Initialize>();
	server.postMethodCall<requests::Initialize>();

	if(state == State::Shutdown)
		server.preMethodCall<requests::Shutdown>();
	else if(state == State::Inactive)
		server.preMethodCall<notifications::Exit>();
}

/*
 * Counts pre/post method calls and exposes the protected message hook.
 */
class HookedEndpoint : public EndpointBase{
public:
	using EndpointBase::EndpointBase;

	int preCount  = 0;
	int postCount = 0;

	template<typename M>
	void preMethodCall(){ ++preCount; }

	template<typename M>
	void postMethodCall(){ ++postCount; }

	template<typename M, typename F>
	void withHook(F&& f)
	{
		const auto hook = messageHook<M>(*this);
		f();
	}
};

template<typename F>
void expectRequestError(F&& f, int expectedCode, std::string_view expectedMessage)
{
	try
	{
		f();
		test::fail("Expected RequestError to be thrown");
	}
	catch(const RequestError& e)
	{
		test::compare(e.code(), expectedCode);
		test::compare(e.message(), expectedMessage);
	}
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * EndpointBase (exercised through ServerEndpointBase, the concrete vehicle,
	 * since bare EndpointBase can't leave the Uninitialized state via public API)
	 */

	app.addTest("EndpointBase/StartsUninitialized", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		test::check(server.isActive(), "active");
		test::check(!server.isInitialized(), "uninitialized");
	});

	app.addTest("EndpointBase/IsActive", [](State state, bool expected){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, state);
		test::compare(server.isActive(), expected);
	})({
		{"Uninitialized", {State::Uninitialized, true }},
		{"Active",        {State::Active,        true }},
		{"Shutdown",      {State::Shutdown,      true }},
		{"Inactive",      {State::Inactive,      false}},
	});

	app.addTest("EndpointBase/ProcessNextMessageSwallowsConnectionErrorWhenInactive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		// No more messages are expected once inactive, so a dropped connection is not an error
		gotoState(server, State::Inactive);
		server.processNextMessage();

		test::compare(stream.readCount, 1);
	});

	app.addTest("EndpointBase/ProcessNextMessageRethrowsAndDeactivatesWhenActive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Active);
		test::expectException<ConnectionError>([&](){ server.processNextMessage(); });

		test::compare(stream.readCount, 1);

		// A connection error while active marks the endpoint inactive before propagating
		test::check(!server.isActive(), "inactive");
	});

	app.addTest("EndpointBase/RunMessageLoopReturnsImmediatelyWhenInactive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Inactive);
		server.runMessageLoop();

		test::compare(stream.readCount, 0);
	});

	app.addTest("EndpointBase/RunMessageLoopRethrowsWhileActive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Active);
		test::expectException<ConnectionError>([&](){ server.runMessageLoop(); });

		// The connection error propagates out of the loop, leaving the endpoint inactive
		test::check(!server.isActive(), "inactive");
	});

	app.addTest("EndpointBase/RunMessageLoopSwallowsConnectionErrorAfterExit", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Active);

		// Simulate the connection dropping right as the endpoint transitions to Inactive
		stream.onRead = [&server](){ server.preMethodCall<notifications::Exit>(); };

		server.runMessageLoop();

		test::compare(stream.readCount, 1);
	});

	app.addTest("EndpointBase/RunMessageLoopProcessesUntilInactive", [](){
		auto stream  = LoopbackStream();
		auto server  = ServerEndpointBase(stream);
		auto handled = 0;

		gotoState(server, State::Active);

		server.messageHandler().onCustom<GenericNotification>("test/stop", [&](json::Value&&)
		{
			++handled;
			server.preMethodCall<notifications::Exit>();
		});

		server.messageHandler().sendNotification("test/stop", json::Value(json::Object()));
		server.runMessageLoop();

		test::compare(handled, 1);
		test::check(!server.isActive(), "inactive");
	});

	app.addTest("EndpointBase/MessageHookCallsPreAndPost", [](){
		auto stream   = LoopbackStream();
		auto endpoint = HookedEndpoint(stream);

		endpoint.withHook<DummyMessage>([&]()
		{
			test::compare(endpoint.preCount, 1);
			test::compare(endpoint.postCount, 0);
		});

		test::compare(endpoint.preCount, 1);
		test::compare(endpoint.postCount, 1);
	});

	/*
	 * ClientEndpointBase
	 */

	app.addTest("Client/GenericCallRequiresInitialize", [](){
		auto stream = LoopbackStream();
		auto client = ClientEndpointBase(stream);

		test::expectException<std::logic_error>(
			[&](){ client.preMethodCall<DummyMessage>(); },
			"Initialize request must be sent first");
	});

	app.addTest("Client/InitializeActivates", [](){
		auto stream = LoopbackStream();
		auto client = ClientEndpointBase(stream);

		client.postMethodCall<requests::Initialize>();
		test::check(client.isActive(), "active");

		// Generic calls are allowed once active
		client.preMethodCall<DummyMessage>();
	});

	app.addTest("Client/ShutdownBlocksFurtherCalls", [](){
		auto stream = LoopbackStream();
		auto client = ClientEndpointBase(stream);

		client.postMethodCall<requests::Initialize>();
		client.preMethodCall<requests::Shutdown>();
		test::check(client.isActive(), "still active after shutdown");

		test::expectException<std::logic_error>(
			[&](){ client.preMethodCall<DummyMessage>(); },
			"Only 'exit' request must be sent after 'shutdown'");
	});

	app.addTest("Client/ExitResetsToInactive", [](){
		auto stream = LoopbackStream();
		auto client = ClientEndpointBase(stream);

		client.postMethodCall<requests::Initialize>();
		client.preMethodCall<notifications::Exit>();

		test::check(!client.isActive(), "inactive");

		// Generic calls are rejected again once inactive
		test::expectException<std::logic_error>(
			[&](){ client.preMethodCall<DummyMessage>(); },
			"Initialize request must be sent first");
	});

	/*
	 * ServerEndpointBase
	 */

	app.addTest("Server/StartsUninitialized", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		test::check(!server.isInitialized(), "!isInitialized");
	});

	app.addTest("Server/GenericCallBeforeInitializeIsRejected", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		expectRequestError(
			[&](){ server.preMethodCall<DummyMessage>(); },
			MessageError::ServerNotInitialized, "Server not initialized");
	});

	app.addTest("Server/InitializeThenActive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		server.preMethodCall<requests::Initialize>();
		test::check(!server.isInitialized(), "notInitializedUntilPost");

		server.postMethodCall<requests::Initialize>();
		test::check(server.isInitialized(), "isInitialized");
		test::check(server.isActive(), "active");

		// Generic calls are allowed once active
		server.preMethodCall<DummyMessage>();
	});

	app.addTest("Server/DoubleInitializeIsRejected", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Active);

		expectRequestError(
			[&](){ server.preMethodCall<requests::Initialize>(); },
			MessageError::InvalidRequest, "Server already initialized");
	});

	app.addTest("Server/ShutdownRequiresInitialize", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		expectRequestError(
			[&](){ server.preMethodCall<requests::Shutdown>(); },
			MessageError::ServerNotInitialized, "Server not initialized");
	});

	app.addTest("Server/ShutdownBlocksFurtherCalls", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Shutdown);
		test::check(server.isActive(), "still active after shutdown");

		expectRequestError(
			[&](){ server.preMethodCall<DummyMessage>(); },
			MessageError::InvalidRequest, "Server has received shutdown request");
	});

	app.addTest("Server/ExitDeactivates", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		gotoState(server, State::Inactive);

		test::check(!server.isActive(), "inactive");
	});

	return app.main(argc, argv);
}
