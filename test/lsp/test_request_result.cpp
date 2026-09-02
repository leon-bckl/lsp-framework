#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <test/test.h>
#include <lsp/request_result.h>

using namespace lsp;

namespace{

// Move-only type to make sure ownership is transferred
using TestResult = std::unique_ptr<int>;

auto syncResult(std::unique_ptr<int> value) -> RequestResult<TestResult>
{
	return RequestResult<TestResult>(std::move(value), 1);
}

auto asyncResult(std::future<std::unique_ptr<int>> future) -> RequestResult<TestResult>
{
	return RequestResult<TestResult>(std::move(future), 1);
}

auto taskResult(TaskFunction<std::unique_ptr<int>> task) -> RequestResult<TestResult>
{
	return RequestResult<TestResult>(std::move(task), 1);
}

auto readyFuture(int value) -> std::future<std::unique_ptr<int>>
{
	auto promise = std::promise<std::unique_ptr<int>>();
	promise.set_value(std::make_unique<int>(value));
	return promise.get_future();
}

} // namespace

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("RequestResult::SyncConstruction", [](){
		auto result = syncResult(std::make_unique<int>(42));
		test::check(!result.isDeferred());
		test::check(result.wait(), "waitBlocking");
		test::check(result.wait(0), "waitTimed");
		test::compare(std::get<json::Integer>(result.requestId()), 1);
		test::compare(*result.get(), 42);
	});

	app.addTest("RequestResult::AsyncConstruction", [](){
		auto result = asyncResult(readyFuture(99));
		test::check(result.isDeferred());
		test::check(result.wait(), "waitReady");
		test::compare(*result.get(), 99);
	});

	app.addTest("RequestResult::TaskConstruction", [](){
		auto result = taskResult(TaskFunction<std::unique_ptr<int>>([]{ return std::make_unique<int>(55); }));
		test::check(result.isDeferred(), "isDeferred");
		test::check(result.isReady(), "readyImmediately");
		test::check(result.wait(0), "waitTimed");
		test::compare(std::get<json::Integer>(result.requestId()), 1);
		test::compare(*result.get(), 55);
	});

	app.addTest("RequestResult::TaskRunsLazilyOnGet", [](){
		auto ran    = false;
		auto result = taskResult(TaskFunction<std::unique_ptr<int>>([&]{ ran = true; return std::make_unique<int>(3); }));

		test::check(!ran, "notRunBeforeGet");
		test::compare(*result.get(), 3);
		test::check(ran, "runByGet");
	});

	app.addTest("RequestResult::TaskExceptionPropagatesOnGet", [](){
		auto result = taskResult(TaskFunction<std::unique_ptr<int>>([]() -> std::unique_ptr<int> { throw std::runtime_error("boom"); }));
		test::expectException<std::runtime_error>([&](){ (void)result.get(); }, "boom");
	});

	app.addTest("RequestResult::IsReady", [](){
		auto sync = syncResult(std::make_unique<int>(1));
		test::check(sync.isReady(), "syncAlwaysReady");

		auto promise = std::promise<std::unique_ptr<int>>();
		auto async   = asyncResult(promise.get_future());

		test::check(!async.isReady(), "asyncNotReadyBeforeValueSet");
		promise.set_value(std::make_unique<int>(3));
		test::check(async.isReady(), "asyncReadyAfterValueSet");

		test::compare(*async.get(), 3);
	});

	app.addTest("RequestResult::AsyncTimedWaitReportsReadiness", [](){
		auto promise = std::promise<std::unique_ptr<int>>();
		auto result  = asyncResult(promise.get_future());

		test::check(!result.wait(0), "notReadyBeforeValueSet");
		promise.set_value(std::make_unique<int>(5));
		test::check(result.wait(0), "readyAfterValueSet");

		test::compare(*result.get(), 5);
	});

	app.addTest("RequestResult::AsyncUntimedWaitBlocksUntilReady", [](){
		auto promise = std::promise<std::unique_ptr<int>>();
		auto result  = asyncResult(promise.get_future());

		auto worker = std::thread([&](){
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			promise.set_value(std::make_unique<int>(7));
		});

		test::check(result.wait(), "returnsOnlyAfterValueSet");
		worker.join();
		test::compare(*result.get(), 7);
	});

	app.addTest("RequestResult::AsyncExceptionPropagatesOnGet", [](){
		auto promise = std::promise<std::unique_ptr<int>>();
		promise.set_exception(std::make_exception_ptr(std::runtime_error("boom")));
		auto result = asyncResult(promise.get_future());

		test::check(result.wait());
		test::expectException<std::runtime_error>([&](){ (void)result.get(); }, "boom");
	});

	return app.main(argc, argv);
}
