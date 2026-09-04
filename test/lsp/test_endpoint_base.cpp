#include <string>
#include <string_view>
#include <test/test.h>
#include "loopback_stream.h"
#include <lsp/connection.h>
#include <lsp/endpoint_base.h>
#include <lsp/error.h>
#include <lsp/json/json.h>
#include <lsp/message_base.h>
#include <lsp/message_handler.h>

using namespace lsp;
using lsptest::LoopbackStream;

namespace{

struct DummyMessage{};

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
		stream.close();
		gotoState(server, State::Inactive);
		server.processNextMessage();

		test::compare(stream.readCount, 1);
	});

	app.addTest("EndpointBase/ProcessNextMessageRethrowsAndDeactivatesWhenActive", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);

		stream.close();
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

		stream.close();
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
		stream.close();
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

	/*
	 * Custom messages
	 */

	app.addTest("Custom/RequestRoundTrip", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);
		gotoState(server, State::Active);

		auto receivedPing = 0;
		server.onCustomRequest("test/echo", [&](json::Value&& params) -> json::Value
		{
			receivedPing = params.object().get("ping").integer();
			return json::Object({{"pong", receivedPing}});
		});

		auto pong      = 0;
		auto gotResult = false;
		server.customRequest("test/echo", json::Object({{"ping", 7}}),
			[&](json::Value&& result){ pong = result.object().get("pong").integer(); gotResult = true; });

		server.processNextMessage(); // handle the request, send the response
		server.processNextMessage(); // handle the response, invoke the callback

		test::check(gotResult, "gotResult");
		test::compare(receivedPing, 7);
		test::compare(pong, 7);
	});

	app.addTest("Custom/RequestNoParamsRoundTrip", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);
		gotoState(server, State::Active);

		auto handled = false;
		server.onCustomRequest("test/ping", [&]() -> json::Value
		{
			handled = true;
			return json::Value(json::Integer(1));
		});

		auto result = 0;
		server.customRequest("test/ping", [&](json::Value&& r){ result = r.integer(); });

		server.processNextMessage();
		server.processNextMessage();

		test::check(handled, "handled");
		test::compare(result, 1);
	});

	app.addTest("Custom/NotificationRoundTrip", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);
		gotoState(server, State::Active);

		auto received = 0;
		auto notified = false;
		server.onCustomNotification("test/touch", [&](json::Value&& params)
		{
			received = params.object().get("v").integer();
			notified = true;
		});

		server.customNotification("test/touch", json::Object({{"v", 3}}));
		server.processNextMessage();

		test::check(notified, "notified");
		test::compare(received, 3);
	});

	app.addTest("Custom/NotificationNoParamsRoundTrip", [](){
		auto stream = LoopbackStream();
		auto server = ServerEndpointBase(stream);
		gotoState(server, State::Active);

		auto notified = false;
		server.onCustomNotification("test/tick", [&](){ notified = true; });

		server.customNotification("test/tick");
		server.processNextMessage();

		test::check(notified, "notified");
	});

	return app.main(argc, argv);
}
