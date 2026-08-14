#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>
#include <test/test.h>
#include <lsp/threadpool.h>

using namespace lsp;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("AddTaskReturnsResult", [](){
		auto pool   = ThreadPool();
		auto future = pool.addTask([](){ return 42; });
		test::compare(future.get(), 42);
	});

	app.addTest("AddTaskWithArgs", [](){
		auto pool   = ThreadPool();
		auto future = pool.addTask([](int a, int b){ return a + b; }, 3, 4);
		test::compare(future.get(), 7);
	});

	app.addTest("AddTaskVoidReturn", [](){
		auto pool     = ThreadPool();
		auto executed = std::atomic<bool>(false);
		auto future   = pool.addTask([&](){ executed = true; });
		future.wait();
		test::check(executed.load());
	});

	app.addTest("MultipleTasksAllComplete", [](){
		auto pool    = ThreadPool();
		auto futures = std::vector<std::future<int>>();

		for(int i = 0; i < 10; ++i)
			futures.push_back(pool.addTask([i](){ return i * i; }));

		auto results  = std::vector<int>();
		auto expected = std::vector<int>();

		for(int i = 0; i < 10; ++i)
		{
			results.push_back(futures[static_cast<std::size_t>(i)].get());
			expected.push_back(i * i);
		}

		test::compare(results, expected);
	});

	app.addTest("ExceptionPropagatesThroughFuture", [](){
		auto pool   = ThreadPool();
		auto future = pool.addTask([](){ throw std::runtime_error("boom"); });
		test::expectException<std::runtime_error>([&](){ future.get(); }, "boom");
	});

	app.addTest("WaitUntilFinishedRunsAllQueuedTasks", [](){
		auto pool    = ThreadPool();
		auto counter = std::atomic<int>(0);

		constexpr int numTasks = 20;

		for(int i = 0; i < numTasks; ++i)
			pool.addTask([&](){ counter.fetch_add(1); });

		pool.waitUntilFinished();
		test::compare(counter.load(), numTasks);
	});

	app.addTest("PoolIsReusableAfterWaitUntilFinished", [](){
		auto pool    = ThreadPool();
		auto counter = std::atomic<int>(0);

		pool.addTask([&](){ counter.fetch_add(1); });
		pool.waitUntilFinished();
		test::compare(counter.load(), 1);

		pool.addTask([&](){ counter.fetch_add(1); });
		pool.waitUntilFinished();
		test::compare(counter.load(), 2);
	});

	app.addTest("DestructorWaitsForQueuedTasks", [](){
		auto counter = std::atomic<int>(0);

		constexpr int numTasks = 20;

		{
			auto pool = ThreadPool();

			for(int i = 0; i < numTasks; ++i)
				pool.addTask([&](){ counter.fetch_add(1); });
		}

		test::compare(counter.load(), numTasks);
	});

	app.addTest("NeverExceedsMaxThreads", [](){
		constexpr auto maxThreads = 3u;
		auto           pool       = ThreadPool(0, maxThreads);

		auto current       = std::atomic<int>(0);
		auto maxConcurrent = std::atomic<int>(0);

		constexpr int numTasks = 12;
		auto          futures  = std::vector<std::future<void>>();

		for(int i = 0; i < numTasks; ++i)
		{
			futures.push_back(pool.addTask([&](){
				const auto concurrent = current.fetch_add(1) + 1;

				auto prevMax = maxConcurrent.load();
				while(concurrent > prevMax && !maxConcurrent.compare_exchange_weak(prevMax, concurrent)){}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				current.fetch_sub(1);
			}));
		}

		for(auto& future : futures)
			future.get();

		test::check(maxConcurrent.load() <= static_cast<int>(maxThreads), "neverExceedsMaxThreads");
	});

	return app.main(argc, argv);
}
