#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <test/test.h>
#include <lsp/io/stream.h>
#include <lsp/json/json.h>
#include <lsp/message_handler.h>

using namespace lsp;

struct TestRequest{
	static constexpr auto Method = std::string_view("test/request");
	static constexpr auto Kind   = MessageKind::Request;

	using Params = std::unordered_map<std::string, int>;
	using Result = int;
};

struct TestNoParamsRequest{
	static constexpr auto Method = std::string_view("test/noParamsRequest");
	static constexpr auto Kind   = MessageKind::Request;

	using Result = std::vector<int>;
};

struct TestNotification{
	static constexpr auto Method = std::string_view("test/notification");
	static constexpr auto Kind   = MessageKind::Notification;

	using Params = std::vector<int>;
};

struct TestNoParamsNotification{
	static constexpr auto Method = std::string_view("test/noParamsNotification");
	static constexpr auto Kind   = MessageKind::Notification;
};

class LoopbackStream : public io::Stream{
public:
	void read(char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		auto lock = std::unique_lock(m_mutex);

		if(!m_dataAvailable.wait_for(lock, std::chrono::seconds(2), [&](){ return m_buffer.size() - m_readOffset >= size; }))
			throw io::Error("Timed out waiting for data");

		std::memcpy(buffer, m_buffer.data() + m_readOffset, size);
		m_readOffset += size;
	}

	void write(const char* buffer, std::size_t size) override
	{
		{
			auto lock = std::unique_lock(m_mutex);

			if(m_readOffset > 0)
			{
				m_buffer.erase(0, m_readOffset);
				m_readOffset = 0;
			}

			m_buffer.append(buffer, size);
		}

		m_dataAvailable.notify_all();
	}

	[[nodiscard]] bool empty() const
	{
		auto lock = std::unique_lock(m_mutex);
		return m_readOffset >= m_buffer.size();
	}

	[[nodiscard]] std::string takeAll()
	{
		auto lock = std::unique_lock(m_mutex);
		auto result = m_buffer.substr(m_readOffset);
		m_buffer.clear();
		m_readOffset = 0;
		return result;
	}

private:
	mutable std::mutex      m_mutex;
	std::condition_variable m_dataAvailable;
	std::string             m_buffer;
	std::size_t             m_readOffset = 0;
};

std::string makeMessage(std::string_view body)
{
	return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
}

json::Value parseMessageBody(std::string_view rawMessage)
{
	const auto headerEnd = rawMessage.find("\r\n\r\n");
	test::check(headerEnd != std::string_view::npos, "hasHeaderBodySeparator");
	return json::parse(rawMessage.substr(headerEnd + 4));
}

template<typename T>
T getResult(RequestResult<T>& result)
{
	if(!result.wait(2000))
		test::fail("Timed out waiting for future");

	return result.get();
}

template<typename M>
void expectResponseError(RequestResult<M>& result, int expectedCode, std::string_view expectedMessage)
{
	try
	{
		getResult(result);
		test::fail("Expected ResponseError to be thrown");
	}
	catch(const ResponseError& e)
	{
		test::compare(e.code(), expectedCode);
		test::compare(e.message(), expectedMessage);
	}
}

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	/*
	 * Requests
	 */

	app.addTest("Request/Future", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = std::unordered_map<std::string, int>();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			called   = true;
			received = params;
			return 42;
		});

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::unordered_map<std::string, int>{{"x", 1}});
		test::compare(getResult(response), 42);
	});

	app.addTest("Request/Callback", [](){
		auto stream     = LoopbackStream();
		auto handler    = MessageHandler(Connection(stream));
		auto called     = false;
		auto received   = std::unordered_map<std::string, int>();
		auto thenResult = std::optional<int>();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			called   = true;
			received = params;
			return 42;
		});

		handler.sendRequest<TestRequest>({{"x", 1}},
			[&](int result){ thenResult = result; },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::unordered_map<std::string, int>{{"x", 1}});
		test::check(thenResult.has_value(), "hasResult");
		test::compare(*thenResult, 42);
	});

	app.addTest("Request/FutureNoParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{1, 2, 3};
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response), std::vector<int>{1, 2, 3});
	});

	app.addTest("Request/CallbackNoParams", [](){
		auto stream     = LoopbackStream();
		auto handler    = MessageHandler(Connection(stream));
		auto called     = false;
		auto thenResult = std::optional<std::vector<int>>();

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{1, 2, 3};
		});

		handler.sendRequest<TestNoParamsRequest>(
			[&](std::vector<int> result){ thenResult = std::move(result); },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::check(thenResult.has_value(), "hasResult");
		test::compare(*thenResult, std::vector<int>{1, 2, 3});
	});

	/*
	 * Notifications
	 */

	app.addTest("Notification/Params", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = std::vector<int>();

		handler.on<TestNotification>([&](std::vector<int> params)
		{
			called   = true;
			received = params;
		});

		handler.sendNotification<TestNotification>({1, 2, 3});
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received, std::vector<int>{1, 2, 3});
	});

	app.addTest("Notification/NoParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsNotification>([&]()
		{
			called = true;
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(called, "called");
	});

	app.addTest("Notification/CallbackThrows", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsNotification>([&](){
			throw std::runtime_error("boom");
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Notification/MethodNotFound", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(stream.empty(), "noResponseWritten");
	});

	/*
	 * Asynchronous handlers (handler returns a std::future)
	 */

	app.addTest("Async/Success", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			called = true;
			auto promise = std::promise<std::vector<int>>();
			promise.set_value(std::vector<int>{1, 2, 3});
			return promise.get_future();
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response), std::vector<int>{1, 2, 3});
	});

	app.addTest("Async/Deferred", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		const auto mainThread     = std::this_thread::get_id();
		auto deferredThread       = std::promise<std::thread::id>();
		auto deferredThreadFuture = deferredThread.get_future();
		auto release              = std::promise<void>();
		auto releaseFuture        = release.get_future();

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				deferredThread.set_value(std::this_thread::get_id());
				(void)releaseFuture.wait_for(std::chrono::seconds(5));
				return std::vector<int>{4, 5, 6};
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();

		test::check(deferredThreadFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "deferredWorkStarted");
		test::check(deferredThreadFuture.get() != mainThread, "deferredWorkRanOnPoolThread");

		release.set_value();

		handler.processNextMessage();
		test::compare(getResult(response), std::vector<int>{4, 5, 6});
	});

	app.addTest("Async/DynamicResult", [](bool async, int expected){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		const auto mainThread    = std::this_thread::get_id();
		auto handlerThread       = std::promise<std::thread::id>();
		auto handlerThreadFuture = handlerThread.get_future();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>) -> RequestResult<int>
		{
			if(async)
				return std::async(std::launch::deferred, [&]() -> int
				{
					handlerThread.set_value(std::this_thread::get_id());
					return 7;
				});

			handlerThread.set_value(std::this_thread::get_id());
			return 11;
		});

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), expected);

		const auto ranOffMessageLoopThread = handlerThreadFuture.get() != mainThread;
		test::check(ranOffMessageLoopThread == async, "asyncRanOffMessageLoopThread");
	})({
		{"ReturnsValue",  {false, 11}},
		{"ReturnsFuture", {true,  7}},
	});

	app.addTest("Notification/Async", [](){
		auto stream = LoopbackStream();

		const auto mainThread = std::this_thread::get_id();
		auto callbackThread   = std::thread::id();
		auto waitThread       = std::promise<std::thread::id>();
		auto waitThreadFuture = waitThread.get_future();
		auto release          = std::promise<void>();
		auto releaseFuture    = release.get_future();

		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsNotification>([&]() -> std::future<void>
		{
			callbackThread = std::this_thread::get_id();

			return std::async(std::launch::deferred, [&]()
			{
				waitThread.set_value(std::this_thread::get_id());
				(void)releaseFuture.wait_for(std::chrono::seconds(5));
			});
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		const auto waitStarted  = waitThreadFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
		const auto waitThreadId = waitStarted ? waitThreadFuture.get() : mainThread;

		release.set_value();

		test::check(callbackThread == mainThread, "callbackRanSynchronously");
		test::check(waitStarted, "asyncWaitStarted");
		test::check(waitThreadId != mainThread, "waitedOnSeparateThread");
		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Notification/AsyncWithParams", [](){
		auto stream = LoopbackStream();

		const auto mainThread = std::this_thread::get_id();
		auto received         = std::vector<int>();
		auto waitThread       = std::promise<std::thread::id>();
		auto waitThreadFuture = waitThread.get_future();

		auto handler = MessageHandler(Connection(stream));

		// A notification callback may return any future.
		// Only future<void> makes sense. Others are accepted as well but the result is discarded.
		handler.on<TestNotification>([&](std::vector<int> params) -> std::future<int>
		{
			received = std::move(params);

			return std::async(std::launch::deferred, [&]() -> int
			{
				waitThread.set_value(std::this_thread::get_id());
				return 0;
			});
		});

		handler.sendNotification<TestNotification>({1, 2, 3});
		handler.processNextMessage();

		const auto waitStarted = waitThreadFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready;

		test::compare(received, std::vector<int>{1, 2, 3});
		test::check(waitStarted, "asyncWaitStarted");
		test::check(waitStarted && waitThreadFuture.get() != mainThread, "waitedOnSeparateThread");
		test::check(stream.empty(), "noResponseWritten");
	});

	/*
	 * RequestContext
	 */

	app.addTest("RequestContext/UnavailableOutsideRequest", [](){
		test::check(!MessageHandler::RequestContext::tryGet(), "noContextByDefault");
		test::expectException<std::logic_error>([](){ (void)MessageHandler::RequestContext::get(); });
	});

	app.addTest("RequestContext/UnavailableInNotificationHandler", [](){
		auto stream           = LoopbackStream();
		auto handler          = MessageHandler(Connection(stream));
		auto contextInHandler = std::optional<bool>();

		handler.on<TestNoParamsNotification>([&]()
		{
			contextInHandler = MessageHandler::RequestContext::tryGet() != nullptr;
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(contextInHandler.has_value(), "handlerCalled");
		test::check(!*contextInHandler, "noContextInNotificationHandler");
	});

	app.addTest("RequestContext/Id", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto ids     = std::vector<MessageId>();

		handler.on<TestNoParamsRequest>([&]()
		{
			ids.push_back(MessageHandler::RequestContext::get().id());
			return std::vector<int>{};
		});

		auto response1 = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		auto response2 = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(ids.size(), 2);
		test::compare(ids[0], response1.requestId());
		test::compare(ids[1], response2.requestId());
		test::check(!MessageHandler::RequestContext::tryGet(), "contextCleared");
	});

	app.addTest("RequestContext/IdInResponseThenCallback", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&](){ return std::vector<int>{}; });

		auto thenId = std::optional<MessageId>();

		const auto requestId = handler.sendRequest<TestNoParamsRequest>(
			[&](const std::vector<int>&)
			{
				thenId = MessageHandler::RequestContext::get().id();
			},
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(thenId.has_value(), "hasThenId");
		test::compare(*thenId, requestId);
		test::check(!MessageHandler::RequestContext::tryGet(), "contextCleared");
	});

	app.addTest("RequestContext/IdInResponseErrorCallback", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			throw RequestError(1234, "custom error");
		});

		auto errorId = std::optional<MessageId>();

		const auto requestId = handler.sendRequest<TestNoParamsRequest>(
			[](const std::vector<int>&){ test::fail("Expected no result"); },
			[&](const ResponseError&)
			{
				errorId = MessageHandler::RequestContext::get().id();
			});

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(errorId.has_value(), "hasErrorId");
		test::compare(*errorId, requestId);
		test::check(!MessageHandler::RequestContext::tryGet(), "contextCleared");
	});

	app.addTest("RequestContext/IdInAsyncRequestHandler", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		auto deferredContextId       = std::promise<std::optional<MessageId>>();
		auto deferredContextIdFuture = deferredContextId.get_future();

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				const auto* context = MessageHandler::RequestContext::tryGet();
				deferredContextId.set_value(
					context ? std::optional<MessageId>(context->id()) : std::nullopt);
				return std::vector<int>{1, 2, 3};
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), std::vector<int>{1, 2, 3});

		test::check(deferredContextIdFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "deferredWorkRan");

		const auto contextId = deferredContextIdFuture.get();
		test::check(contextId.has_value(), "contextSetInDeferredWork");
		test::compare(*contextId, response.requestId());
		test::check(!MessageHandler::RequestContext::tryGet(), "contextCleared");
	});

	/*
	 * Errors
	 */

	app.addTest("Error/Future", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			if(useRequestError)
				throw RequestError(1234, "custom error");

			throw std::runtime_error("boom");
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, expectedCode, expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/Callback", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			if(useRequestError)
				throw RequestError(1234, "custom error");

			throw std::runtime_error("boom");
		});

		auto thenCalled  = false;
		auto errorResult = std::optional<ResponseError>();

		handler.sendRequest<TestNoParamsRequest>(
			[&](const std::vector<int>&){ thenCalled = true; },
			[&](const ResponseError& e){ errorResult = e; });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(!thenCalled, "thenNotCalled");
		test::check(errorResult.has_value(), "hasError");
		test::compare(errorResult->code(), expectedCode);
		test::compare(errorResult->message(), expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/Async", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			auto promise = std::promise<std::vector<int>>();

			if(useRequestError)
				promise.set_exception(std::make_exception_ptr(RequestError(1234, "custom error")));
			else
				promise.set_exception(std::make_exception_ptr(std::runtime_error("boom")));

			return promise.get_future();
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, expectedCode, expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/MethodNotFound", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, MessageError::MethodNotFound, "Method not found");
	});

	app.addTest("Error/Data", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			throw RequestError(1234, "custom error", json::Value(json::Object({{"detail", "x"}})));
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		try
		{
			getResult(response);
			test::fail("Expected ResponseError to be thrown");
		}
		catch(const ResponseError& e)
		{
			test::compare(e.code(), 1234);
			test::check(e.data().has_value(), "hasData");
			test::compare(e.data()->object().get("detail").string(), "x");
		}
	});

	app.addTest("Error/InvalidParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>)
		{
			return 42;
		});

		const auto message = makeMessage(R"({"jsonrpc":"2.0","id":1,"method":"test/request","params":{"x":"bad"}})");
		stream.write(message.data(), message.size());

		handler.processNextMessage();

		const auto response = parseMessageBody(stream.takeAll());
		test::compare(response.object().get("error").object().get("code").integer(), MessageError::InvalidParams);
	});

	/*
	 * Custom (generic) methods
	 */

	app.addTest("Generic/Request", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = json::Value();

		handler.onCustom<GenericRequest>("generic/request", [&](json::Value&& params) -> json::Value
		{
			called   = true;
			received = params;
			return json::Value(json::Integer(42));
		});

		auto response = handler.sendCustomRequest<GenericRequest>("generic/request", json::Value(json::Object({{"x", 1}})));
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received.object().get("x").integer(), 1);
		test::compare(getResult(response).integer(), 42);
	});

	app.addTest("Generic/Async", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.onCustom<GenericRequest>("generic/asyncRequest", [&](json::Value&&) -> std::future<GenericRequest::Result>
		{
			called = true;
			auto promise = std::promise<json::Value>();
			promise.set_value(json::Value(json::Integer(42)));
			return promise.get_future();
		});

		auto response = handler.sendCustomRequest<GenericRequestNoParams>("generic/asyncRequest");
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(getResult(response).integer(), 42);
	});

	app.addTest("Generic/RequestCallback", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.onCustom<GenericRequest>("generic/requestCallback", [&](json::Value&& params) -> json::Value
		{
			called = true;
			return json::Value(json::Integer(params.object().get("x").integer() * 2));
		});

		auto thenResult = std::optional<json::Value>();

		handler.sendCustomRequest<GenericRequest>("generic/requestCallback", json::Value(json::Object({{"x", 21}})),
			[&](json::Value&& result){ thenResult = std::move(result); },
			[](const ResponseError&){ test::fail("Expected no error"); });

		handler.processNextMessage();
		handler.processNextMessage();

		test::check(called, "called");
		test::check(thenResult.has_value(), "hasResult");
		test::compare(thenResult->integer(), 42);
	});

	app.addTest("Generic/Notification", [](){
		auto stream   = LoopbackStream();
		auto handler  = MessageHandler(Connection(stream));
		auto called   = false;
		auto received = json::Value();

		handler.onCustom<GenericNotification>("generic/notification", [&](json::Value&& params)
		{
			called   = true;
			received = params;
		});

		handler.sendNotification("generic/notification", json::Value(json::Object({{"x", 1}})));
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received.object().get("x").integer(), 1);
	});

	/*
	 * Handler registration and multiplexing
	 */

	app.addTest("Remove", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			called = true;
			return std::vector<int>{};
		});

		handler.remove(std::string(TestNoParamsRequest::Method));

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::check(!called, "notCalled");
		expectResponseError(response, MessageError::MethodNotFound, "Method not found");
	});

	app.addTest("MultipleInFlightRequests", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			return params.at("x");
		});

		auto responseA = handler.sendRequest<TestRequest>({{"x", 1}});
		auto responseB = handler.sendRequest<TestRequest>({{"x", 2}});

		handler.processNextMessage();
		handler.processNextMessage();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(responseA), 1);
		test::compare(getResult(responseB), 2);
	});

	app.addTest("Batch", [](){
		auto stream              = LoopbackStream();
		auto handler             = MessageHandler(Connection(stream));
		auto requestCalled       = false;
		auto paramsRequestCalled = false;
		auto notificationCalled  = false;

		handler.on<TestNoParamsRequest>([&]()
		{
			requestCalled = true;
			return std::vector<int>{1, 2, 3};
		});

		handler.on<TestRequest>([&](std::unordered_map<std::string, int> params)
		{
			paramsRequestCalled = true;
			return params.at("x") * 2;
		});

		handler.on<TestNoParamsNotification>([&]()
		{
			notificationCalled = true;
		});

		const auto batchBody = std::string(
			R"([{"jsonrpc":"2.0","id":1,"method":"test/noParamsRequest"},)"
			R"({"jsonrpc":"2.0","method":"test/noParamsNotification"},)"
			R"({"jsonrpc":"2.0","id":2,"method":"test/request","params":{"x":5}}])");
		const auto message = makeMessage(batchBody);
		stream.write(message.data(), message.size());

		handler.processNextMessage(); // The whole batch arrives as a single message

		test::check(requestCalled, "requestCalled");
		test::check(paramsRequestCalled, "paramsRequestCalled");
		test::check(notificationCalled, "notificationCalled");

		const auto response = parseMessageBody(stream.takeAll());
		test::check(response.isArray(), "isArray");
		test::compare(response.array().size(), 2);
		test::compare(response.array()[0].object().get("id").integer(), 1);
		test::compare(response.array()[0].object().get("result").array().size(), 3);
		test::compare(response.array()[1].object().get("id").integer(), 2);
		test::compare(response.array()[1].object().get("result").integer(), 10);
	});

	return app.main(argc, argv);
}
