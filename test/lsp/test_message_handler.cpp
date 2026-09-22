#include <chrono>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <test/test.h>
#include "loopback_stream.h"
#include <lsp/json/json.h>
#include <lsp/message_handler.h>

using namespace lsp;
using lsptest::LoopbackStream;

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

template<typename M>
void expectCallbackError(MessageHandler& handler, int expectedCode, std::string_view expectedMessage)
{
	auto thenCalled  = false;
	auto errorResult = std::optional<ResponseError>();

	handler.sendRequest<M>(
		[&](const typename M::Result&){ thenCalled = true; },
		[&](const ResponseError& e){ errorResult = e; });

	handler.processNextMessage();
	handler.processNextMessage();

	test::check(!thenCalled, "thenNotCalled");
	test::check(errorResult.has_value(), "hasError");
	test::compare(errorResult->code(), expectedCode);
	test::compare(errorResult->message(), expectedMessage);
}

void respondWithNonArray(MessageHandler& handler)
{
	handler.onCustom<GenericRequestNoParams>(TestNoParamsRequest::Method, []() -> json::Value
	{
		return json::Value(json::String("not an array"));
	});
}

/*
 * Synchronizes a std::async(std::launch::deferred, ...) task with the test.
 */
class DeferredGate{
public:
	void started(){ m_started.set_value(); }
	void waitForRelease(){ (void)m_releasedFuture.wait_for(std::chrono::seconds(5)); }

	void release(){ m_released.set_value(); }
	void waitForStart()
	{
		test::check(m_startedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "workStarted");
	}

private:
	std::promise<void> m_started;
	std::future<void>  m_startedFuture  = m_started.get_future();
	std::promise<void> m_released;
	std::future<void>  m_releasedFuture = m_released.get_future();
};

/*
 * A copy of MessageHandler::MessageLog that owns its strings, since the original
 * holds string_views into data that is only valid for the duration of the callback.
 */
struct LoggedMessage{
	std::string                 direction; // "in" or "out"
	std::string                 kind;      // "request", "notification" or "response"
	std::string                 method;
	std::optional<RequestId>    id;
	bool                        hasDuration;
	std::optional<int>          errorCode;
	std::optional<std::string>  errorMessage;
	std::optional<std::string>  payload;
};

LoggedMessage toLoggedMessage(const MessageHandler::MessageLog& log)
{
	auto logged = LoggedMessage{
		.direction   = log.incoming ? "in" : "out",
		.kind        = log.isNotification() ? "notification" : log.isResponse() ? "response" : "request",
		.method      = std::string(log.method),
		.id          = log.id,
		.hasDuration = log.requestDuration.has_value(),
		.payload     = log.payload,
	};

	if(log.error.has_value())
	{
		logged.errorCode    = log.error->code;
		logged.errorMessage = std::string(log.error->message);
	}

	return logged;
}

void expectLogKind(const LoggedMessage& log, std::string_view direction, std::string_view kind)
{
	test::compare(log.direction, std::string(direction));
	test::compare(log.kind, std::string(kind));
}

void expectLogError(const LoggedMessage& log, int expectedCode, std::string_view expectedMessage)
{
	test::check(log.errorCode.has_value(), "hasErrorCode");
	test::compare(*log.errorCode, expectedCode);
	test::check(log.errorMessage.has_value(), "hasErrorMessage");
	test::compare(*log.errorMessage, std::string(expectedMessage));
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

	app.addTest("Notification/InvalidParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto called  = false;

		handler.on<TestNotification>([&](std::vector<int>){ called = true; });

		const auto message = makeMessage(R"({"jsonrpc":"2.0","method":"test/notification","params":{"nope":1}})");
		stream.write(message.data(), message.size());
		handler.processNextMessage();

		test::check(!called, "callbackNotCalled");
		test::check(stream.empty(), "noResponseWritten");

		handler.sendNotification<TestNotification>({1, 2, 3});
		handler.processNextMessage();
		test::check(called, "laterNotificationHandled");
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

	app.addTest("Async/TaskFunction", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		const auto mainThread = std::this_thread::get_id();
		auto taskThread       = std::promise<std::thread::id>();
		auto taskThreadFuture = taskThread.get_future();

		handler.on<TestNoParamsRequest>([&]() -> TaskFunction<TestNoParamsRequest::Result>
		{
			return TaskFunction<TestNoParamsRequest::Result>([&]() -> std::vector<int>
			{
				taskThread.set_value(std::this_thread::get_id());
				return std::vector<int>{4, 5, 6};
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), std::vector<int>{4, 5, 6});
		test::check(taskThreadFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "taskRan");
		test::check(taskThreadFuture.get() != mainThread, "ranOnWorkerThread");
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

	app.addTest("Notification/AsyncCallable", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		const auto mainThread           = std::this_thread::get_id();
		auto       handlerThread        = std::thread::id();
		auto       callableThread       = std::promise<std::thread::id>();
		auto       callableThreadFuture = callableThread.get_future();

		handler.on<TestNoParamsNotification>([&]() -> TaskFunction<void>
		{
			handlerThread = std::this_thread::get_id();
			return TaskFunction<void>([&]{ callableThread.set_value(std::this_thread::get_id()); });
		});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		const auto ran = callableThreadFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready;

		test::check(handlerThread == mainThread, "handlerRanSynchronously");
		test::check(ran, "callableRan");
		test::check(ran && callableThreadFuture.get() != mainThread, "callableRanOnWorkerThread");
		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Notification/AsyncCallableWithParams", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		auto received       = std::promise<std::vector<int>>();
		auto receivedFuture = received.get_future();

		handler.on<TestNotification>([&](std::vector<int> params) -> TaskFunction<void>
		{
			return TaskFunction<void>([&received, params = std::move(params)]() mutable
			{
				received.set_value(std::move(params));
			});
		});

		handler.sendNotification<TestNotification>({1, 2, 3});
		handler.processNextMessage();

		test::check(receivedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "callableRan");
		test::compare(receivedFuture.get(), std::vector<int>{1, 2, 3});
		test::check(stream.empty(), "noResponseWritten");
	});

	app.addTest("Notification/SyncHandlerRunsOnMessageLoopThread", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		const auto mainThread    = std::this_thread::get_id();
		auto       handlerThread = std::thread::id();

		handler.on<TestNoParamsNotification>([&]{ handlerThread = std::this_thread::get_id(); });

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(handlerThread == mainThread, "ranSynchronously");
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
		auto ids     = std::vector<RequestId>();

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

		auto thenId = std::optional<RequestId>();

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

		auto errorId = std::optional<RequestId>();

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

		auto deferredContextId       = std::promise<std::optional<RequestId>>();
		auto deferredContextIdFuture = deferredContextId.get_future();

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				const auto* context = MessageHandler::RequestContext::tryGet();
				deferredContextId.set_value(
					context ? std::optional<RequestId>(context->id()) : std::nullopt);
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
	 * Cancelation
	 */

	app.addTest("Cancelation/UnknownIdIsNotCanceled", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		test::check(!handler.isCanceled(RequestId(json::Integer(999))), "notCanceled");

		handler.cancel(RequestId(json::Integer(999)));
		test::check(!handler.isCanceled(RequestId(json::Integer(999))), "stillNotCanceledAfterCancel");
	});

	app.addTest("Cancelation/ActiveRequestObservesCancelViaIsCanceled", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto gate    = DeferredGate();

		auto canceledBefore = false;
		auto canceledAfter  = false;

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				canceledBefore = MessageHandler::RequestContext::get().isCanceled();
				gate.started();
				gate.waitForRelease();
				canceledAfter = MessageHandler::RequestContext::get().isCanceled();
				return std::vector<int>{};
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();

		gate.waitForStart();

		handler.cancel(response.requestId());
		gate.release();

		handler.processNextMessage();
		test::compare(getResult(response), std::vector<int>{});

		test::check(!canceledBefore, "notCanceledBeforeCancelCall");
		test::check(canceledAfter, "canceledAfterCancelCall");
	});

	app.addTest("Cancelation/ThrowIfCanceledIsNoOpWhenNotCanceled", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			MessageHandler::RequestContext::get().throwIfCanceled();
			return std::vector<int>{1, 2, 3};
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), std::vector<int>{1, 2, 3});
	});

	app.addTest("Cancelation/ThrowIfCanceledSendsRequestCancelledError", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto gate    = DeferredGate();

		handler.on<TestNoParamsRequest>([&]() -> std::future<TestNoParamsRequest::Result>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				gate.started();
				gate.waitForRelease();
				MessageHandler::RequestContext::get().throwIfCanceled();
				return std::vector<int>{};
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();

		gate.waitForStart();

		handler.cancel(response.requestId());
		gate.release();

		handler.processNextMessage();

		expectResponseError(response, MessageError::RequestCancelled, "Canceled");
	});

	app.addTest("Cancelation/IdIsForgottenAfterRequestCompletes", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&](){ return std::vector<int>{}; });

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), std::vector<int>{});

		// The request already completed, so canceling its id now must be a safe no-op
		handler.cancel(response.requestId());
		test::check(!handler.isCanceled(response.requestId()), "idForgottenAfterCompletion");
	});

	app.addTest("Cancelation/CancelingOneDoesNotAffectAnother", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto gateA   = DeferredGate();
		auto gateB   = DeferredGate();

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>) -> std::future<int>
		{
			return std::async(std::launch::deferred, [&]() -> int
			{
				gateA.waitForRelease();
				return 1;
			});
		});

		handler.on<TestNoParamsRequest>([&]() -> std::future<std::vector<int>>
		{
			return std::async(std::launch::deferred, [&]() -> std::vector<int>
			{
				gateB.waitForRelease();
				return std::vector<int>{};
			});
		});

		auto responseA = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		auto responseB = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();

		handler.cancel(responseA.requestId());

		test::check(handler.isCanceled(responseA.requestId()), "aCanceled");
		test::check(!handler.isCanceled(responseB.requestId()), "bNotCanceled");

		gateA.release();
		gateB.release();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(responseA), 1);
		test::compare(getResult(responseB), std::vector<int>{});
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

		expectCallbackError<TestNoParamsRequest>(handler, expectedCode, expectedMessage);
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

	app.addTest("Error/Task", [](bool useRequestError, int expectedCode, std::string_view expectedMessage){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&]() -> TaskFunction<TestNoParamsRequest::Result>
		{
			return TaskFunction<TestNoParamsRequest::Result>([&]() -> std::vector<int>
			{
				if(useRequestError)
					throw RequestError(1234, "custom error");

				throw std::runtime_error("boom");
			});
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, expectedCode, expectedMessage);
	})({
		{"RequestError",     {true,  1234,                        "custom error"}},
		{"GenericException", {false, MessageError::InternalError, "boom"}},
	});

	app.addTest("Error/MalformedResponseFuture", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		respondWithNonArray(handler);

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, MessageError::InternalError, "JSON value is not array");
	});

	app.addTest("Error/MalformedResponseCallback", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		respondWithNonArray(handler);

		expectCallbackError<TestNoParamsRequest>(handler, MessageError::InternalError, "JSON value is not array");
	});

	app.addTest("Error/ResponseThenCallbackThrows", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestNoParamsRequest>([&](){ return std::vector<int>{1, 2, 3}; });

		auto errorCalled = false;

		handler.sendRequest<TestNoParamsRequest>(
			[&](const std::vector<int>&){ throw json::TypeError("boom in then"); },
			[&](const ResponseError&){ errorCalled = true; });

		handler.processNextMessage();

		test::expectException<json::TypeError>([&](){ handler.processNextMessage(); }, "boom in then");

		test::check(!errorCalled, "errorNotCalled");
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
		test::compare(response.object().get("id").integer(), 1);
		test::check(!response.object().contains("result"), "noResult");
		test::compare(response.object().get("error").object().get("code").integer(), MessageError::InvalidParams);
	});

	app.addTest("Error/RequestHandlerThrowsTypeError", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>) -> int
		{
			throw json::TypeError("type error inside handler");
		});

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, MessageError::InternalError, "type error inside handler");
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

		handler.sendCustomNotification<GenericNotification>("generic/notification", json::Value(json::Object({{"x", 1}})));
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(received.object().get("x").integer(), 1);
	});

	/*
	 * Message log
	 */

	app.addTest("MessageLog/OffByDefault", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		handler.on<TestNoParamsRequest>([&](){ return std::vector<int>{}; });

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), std::vector<int>{});
		test::check(logs.empty(), "noLogsWithoutExplicitLevel");
	});

	app.addTest("MessageLog/RoundTripLogsAllFourEvents", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>){ return 42; });

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), 42);
		test::compare(logs.size(), 4);

		expectLogKind(logs[0], "out", "request");
		expectLogKind(logs[1], "in",  "request");
		expectLogKind(logs[2], "out", "response");
		expectLogKind(logs[3], "in",  "response");

		for(const auto& log : logs)
		{
			test::compare(log.method, std::string(TestRequest::Method));
			test::check(log.id.has_value(), "hasId");
			test::compare(*log.id, response.requestId());
		}

		test::check(!logs[0].hasDuration, "outgoingRequestHasNoDuration");
		test::check(!logs[1].hasDuration, "incomingRequestHasNoDuration");
		test::check(logs[2].hasDuration, "outgoingResponseHasDuration");
		test::check(logs[3].hasDuration, "incomingResponseHasDuration");
	});

	app.addTest("MessageLog/NotificationLoggedBothSides", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		auto called = false;
		handler.on<TestNoParamsNotification>([&](){ called = true; });

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::check(called, "called");
		test::compare(logs.size(), 2);

		expectLogKind(logs[0], "out", "notification");
		test::check(!logs[0].id.has_value(), "outgoingNotificationHasNoId");

		expectLogKind(logs[1], "in", "notification");
		test::check(!logs[1].id.has_value(), "incomingNotificationHasNoId");

		for(const auto& log : logs)
			test::compare(log.method, std::string(TestNoParamsNotification::Method));
	});

	app.addTest("MessageLog/ErrorResponseLogged", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		handler.on<TestNoParamsRequest>([&]() -> std::vector<int>
		{
			throw RequestError(1234, "custom error");
		});

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, 1234, "custom error");

		test::compare(logs.size(), 4);

		test::check(!logs[1].errorCode.has_value(), "incomingRequestHasNoError");

		expectLogKind(logs[2], "out", "response");
		expectLogError(logs[2], 1234, "custom error");

		expectLogKind(logs[3], "in", "response");
		test::check(logs[3].errorCode.has_value(), "incomingResponseHasErrorCode");
		test::compare(*logs[3].errorCode, 1234);
	});

	app.addTest("MessageLog/MethodNotFoundLogsIncomingRequestAndErrorResponse", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		auto response = handler.sendRequest<TestNoParamsRequest>();
		handler.processNextMessage();
		handler.processNextMessage();

		expectResponseError(response, MessageError::MethodNotFound, "Method not found");

		test::compare(logs.size(), 4);

		expectLogKind(logs[1], "in", "request");
		test::check(!logs[1].errorCode.has_value(), "incomingRequestHasNoError");

		expectLogKind(logs[2], "out", "response");
		expectLogError(logs[2], MessageError::MethodNotFound, "Method not found");
	});

	app.addTest("MessageLog/PayloadOnlyAtVerboseLevel", [](MessageHandler::MessageLogLevel level, bool expectPayload){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(level);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>){ return 42; });

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), 42);
		test::compare(logs.size(), 4);

		for(const auto& log : logs)
			test::compare(log.payload.has_value(), expectPayload);
	})({
		{"Info",           {MessageHandler::MessageLogLevel::Info,           false}},
		{"InfoAndPayload", {MessageHandler::MessageLogLevel::InfoAndPayload, true}},
	});

	app.addTest("MessageLog/VerbosePayloadContainsParamsAndResult", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto logs    = std::vector<LoggedMessage>();

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::InfoAndPayload);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog& log){ logs.push_back(toLoggedMessage(log)); });

		handler.on<TestRequest>([&](std::unordered_map<std::string, int>){ return 42; });

		auto response = handler.sendRequest<TestRequest>({{"x", 1}});
		handler.processNextMessage();
		handler.processNextMessage();

		test::compare(getResult(response), 42);
		test::compare(logs.size(), 4);

		test::check(logs[0].payload.has_value(), "outgoingRequestHasPayload");
		test::compare(json::parse(*logs[0].payload).object().get("x").integer(), 1);

		test::check(logs[2].payload.has_value(), "outgoingResponseHasPayload");
		test::compare(json::parse(*logs[2].payload).integer(), 42);
	});

	app.addTest("MessageLog/MultipleCallbacksAllInvoked", [](){
		auto stream  = LoopbackStream();
		auto handler = MessageHandler(Connection(stream));
		auto countA  = 0;
		auto countB  = 0;

		handler.setMessageLogLevel(MessageHandler::MessageLogLevel::Info);
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog&){ ++countA; });
		handler.addMessageLogCallback([&](const MessageHandler::MessageLog&){ ++countB; });

		handler.on<TestNoParamsNotification>([&](){});

		handler.sendNotification<TestNoParamsNotification>();
		handler.processNextMessage();

		test::compare(countA, 2);
		test::compare(countB, 2);
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
