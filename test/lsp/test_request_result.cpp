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
struct TestRequest{
	using Result = std::unique_ptr<int>;
};

auto syncResult(std::unique_ptr<int> value) -> RequestResult<TestRequest>
{
	return RequestResult<TestRequest>(RequestId(json::Integer(1)), std::move(value));
}

auto asyncResult(std::future<std::unique_ptr<int>> future) -> RequestResult<TestRequest>
{
	return RequestResult<TestRequest>(RequestId(json::Integer(1)), std::move(future));
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
		test::check(!result.isAsync());
		test::check(result.wait(), "waitBlocking");
		test::check(result.wait(0), "waitTimed");
		test::compare(std::get<json::Integer>(result.requestId()), 1);
		test::compare(*result.get(), 42);
	});

	app.addTest("RequestResult::AsyncConstruction", [](){
		auto result = asyncResult(readyFuture(99));
		test::check(result.isAsync());
		test::check(result.wait(), "waitReady");
		test::compare(*result.get(), 99);
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
