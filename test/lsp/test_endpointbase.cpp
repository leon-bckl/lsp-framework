#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <test/test.h>
#include <lsp/connection.h>
#include <lsp/endpointbase.h>
#include <lsp/error.h>
#include <lsp/io/stream.h>
#include <lsp/messagebase.h>
#include <lsp/messagehandler.h>

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

/*
 * Makes protected state accessors visible to test.
 */
template<typename Base>
class Exposed : public Base{
public:
	using Base::Base;
	using EndpointBase::State;
	using EndpointBase::state;
	using EndpointBase::setState;
};

using ClientUnderTest = Exposed<ClientEndpointBase>;
using ServerUnderTest = Exposed<ServerEndpointBase>;

/*
 * Also counts pre/post method calls and exposes the protected message hook.
 */
class BaseUnderTest : public Exposed<EndpointBase>{
public:
	using Exposed<EndpointBase>::Exposed;

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
	 * EndpointBase
	 */

	app.addTest("EndpointBase/StartsUninitialized", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		test::check(endpoint.state() == BaseUnderTest::State::Uninitialized, "uninitialized");
	});

	app.addTest("EndpointBase/ProcessNextMessageSkippedWhenInactive", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		endpoint.setState(BaseUnderTest::State::Inactive);
		endpoint.processNextMessage();

		test::compare(stream.readCount, 0);
	});

	app.addTest("EndpointBase/ProcessNextMessageRunsWhenActive", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		endpoint.setState(BaseUnderTest::State::Active);
		test::expectException<ConnectionError>([&](){ endpoint.processNextMessage(); });

		test::compare(stream.readCount, 1);
	});

	app.addTest("EndpointBase/RunMessageLoopReturnsImmediatelyWhenInactive", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		endpoint.setState(BaseUnderTest::State::Inactive);
		endpoint.runMessageLoop();

		test::compare(stream.readCount, 0);
	});

	app.addTest("EndpointBase/RunMessageLoopRethrowsWhileActive", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		endpoint.setState(BaseUnderTest::State::Active);
		test::expectException<ConnectionError>([&](){ endpoint.runMessageLoop(); });
	});

	app.addTest("EndpointBase/RunMessageLoopSwallowsConnectionErrorAfterExit", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

		// Simulate the connection dropping right as the endpoint transitions to Inactive
		stream.onRead = [&](){ endpoint.setState(BaseUnderTest::State::Inactive); };

		endpoint.setState(BaseUnderTest::State::Active);
		endpoint.runMessageLoop();

		test::compare(stream.readCount, 1);
	});

	app.addTest("EndpointBase/RunMessageLoopProcessesUntilInactive", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);
		auto handled  = 0;

		endpoint.messageHandler().onCustom<GenericNotification>("test/stop", [&](json::Value&&)
		{
			++handled;
			endpoint.setState(BaseUnderTest::State::Inactive);
		});

		endpoint.messageHandler().sendNotification("test/stop", json::Value(json::Object()));
		endpoint.runMessageLoop();

		test::compare(handled, 1);
		test::check(endpoint.state() == BaseUnderTest::State::Inactive, "inactive");
	});

	app.addTest("EndpointBase/MessageHookCallsPreAndPost", [](){
		auto stream   = LoopbackStream();
		auto endpoint = BaseUnderTest(stream);

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
		auto client = ClientUnderTest(stream);

		test::expectException<std::logic_error>(
			[&](){ client.preMethodCall<DummyMessage>(); },
			"Initialize request must be sent first");
	});

	app.addTest("Client/InitializeActivates", [](){
		auto stream = LoopbackStream();
		auto client = ClientUnderTest(stream);

		client.preMethodCall<requests::Initialize>();
		test::check(client.state() == ClientUnderTest::State::Active, "active");

		// Generic calls are allowed once active
		client.preMethodCall<DummyMessage>();
	});

	app.addTest("Client/ShutdownBlocksFurtherCalls", [](){
		auto stream = LoopbackStream();
		auto client = ClientUnderTest(stream);

		client.preMethodCall<requests::Initialize>();
		client.preMethodCall<requests::Shutdown>();
		test::check(client.state() == ClientUnderTest::State::Shutdown, "shutdown");

		test::expectException<std::logic_error>(
			[&](){ client.preMethodCall<DummyMessage>(); },
			"Only 'exit' request must be sent after 'shutdown'");
	});

	app.addTest("Client/ExitResetsToUninitialized", [](){
		auto stream = LoopbackStream();
		auto client = ClientUnderTest(stream);

		client.preMethodCall<requests::Initialize>();
		client.preMethodCall<notifications::Exit>();

		test::check(client.state() == ClientUnderTest::State::Uninitialized, "uninitialized");
	});

	/*
	 * ServerEndpointBase
	 */

	app.addTest("Server/StartsUninitialized", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		test::check(!server.isInitialized(), "!isInitialized");
	});

	app.addTest("Server/GenericCallBeforeInitializeIsRejected", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		expectRequestError(
			[&](){ server.preMethodCall<DummyMessage>(); },
			MessageError::ServerNotInitialized, "Server not initialized");
	});

	app.addTest("Server/InitializeThenActive", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		server.preMethodCall<requests::Initialize>();
		test::check(!server.isInitialized(), "notInitializedUntilPost");

		server.postMethodCall<requests::Initialize>();
		test::check(server.isInitialized(), "isInitialized");
		test::check(server.state() == ServerUnderTest::State::Active, "active");

		// Generic calls are allowed once active
		server.preMethodCall<DummyMessage>();
	});

	app.addTest("Server/DoubleInitializeIsRejected", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		server.preMethodCall<requests::Initialize>();
		server.postMethodCall<requests::Initialize>();

		expectRequestError(
			[&](){ server.preMethodCall<requests::Initialize>(); },
			MessageError::InvalidRequest, "Server already initialized");
	});

	app.addTest("Server/ShutdownRequiresInitialize", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		expectRequestError(
			[&](){ server.preMethodCall<requests::Shutdown>(); },
			MessageError::ServerNotInitialized, "Server not initialized");
	});

	app.addTest("Server/ShutdownBlocksFurtherCalls", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		server.preMethodCall<requests::Initialize>();
		server.postMethodCall<requests::Initialize>();
		server.preMethodCall<requests::Shutdown>();
		test::check(server.state() == ServerUnderTest::State::Shutdown, "shutdown");

		expectRequestError(
			[&](){ server.preMethodCall<DummyMessage>(); },
			MessageError::InvalidRequest, "Server has received shutdown request");
	});

	app.addTest("Server/ExitDeactivates", [](){
		auto stream = LoopbackStream();
		auto server = ServerUnderTest(stream);

		server.preMethodCall<requests::Initialize>();
		server.postMethodCall<requests::Initialize>();
		server.preMethodCall<notifications::Exit>();

		test::check(server.state() == ServerUnderTest::State::Inactive, "inactive");
	});

	return app.main(argc, argv);
}
