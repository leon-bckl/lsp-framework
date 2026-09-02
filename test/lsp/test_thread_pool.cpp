#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <test/test.h>
#include <lsp/thread_pool.h>

using namespace lsp;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("ThreadPool/RunsSubmittedTask", [](){
		auto pool = ThreadPool();
		auto ran  = std::atomic<bool>(false);

		pool.addTask([&]{ ran = true; });
		pool.waitUntilFinished();

		test::check(ran.load(), "ran");
	});

	app.addTest("ThreadPool/RunsTaskOnWorkerThread", [](){
		auto pool       = ThreadPool();
		auto mainThread = std::this_thread::get_id();
		auto taskThread = std::thread::id();

		pool.addTask([&]{ taskThread = std::this_thread::get_id(); });
		pool.waitUntilFinished();

		test::check(taskThread != mainThread, "ranOffMainThread");
	});

	app.addTest("ThreadPool/PassesBoundArguments", [](){
		auto pool   = ThreadPool();
		auto result = std::atomic<int>(0);

		pool.addTask(ThreadPool::Task([&](int a, int b){ result = a + b; }, 3, 4));
		pool.waitUntilFinished();

		test::compare(result.load(), 7);
	});

	app.addTest("ThreadPool/RunsTaskWithMoveOnlyCapture", [](){
		auto pool  = ThreadPool();
		auto value = std::atomic<int>(0);

		pool.addTask([&value, p = std::make_unique<int>(42)]{ value = *p; });
		pool.waitUntilFinished();

		test::compare(value.load(), 42);
	});

	app.addTest("ThreadPool/RunsAllQueuedTasks", [](){
		auto pool    = ThreadPool();
		auto counter = std::atomic<int>(0);

		constexpr int numTasks = 20;

		for(int i = 0; i < numTasks; ++i)
			pool.addTask([&]{ counter.fetch_add(1); });

		pool.waitUntilFinished();
		test::compare(counter.load(), numTasks);
	});

	app.addTest("ThreadPool/IsReusableAfterWaitUntilFinished", [](){
		auto pool    = ThreadPool();
		auto counter = std::atomic<int>(0);

		pool.addTask([&]{ counter.fetch_add(1); });
		pool.waitUntilFinished();
		test::compare(counter.load(), 1);

		pool.addTask([&]{ counter.fetch_add(1); });
		pool.waitUntilFinished();
		test::compare(counter.load(), 2);
	});

	app.addTest("ThreadPool/DestructorWaitsForQueuedTasks", [](){
		auto counter = std::atomic<int>(0);

		constexpr int numTasks = 20;

		{
			auto pool = ThreadPool();

			for(int i = 0; i < numTasks; ++i)
				pool.addTask([&]{ counter.fetch_add(1); });
		}

		test::compare(counter.load(), numTasks);
	});

	app.addTest("ThreadPool/NeverExceedsMaxThreads", [](){
		constexpr auto maxThreads = 3u;
		auto           pool       = ThreadPool(0, maxThreads);

		auto current       = std::atomic<int>(0);
		auto maxConcurrent = std::atomic<int>(0);

		constexpr int numTasks = 12;

		for(int i = 0; i < numTasks; ++i)
		{
			pool.addTask([&]{
				const auto concurrent = current.fetch_add(1) + 1;

				auto prevMax = maxConcurrent.load();
				while(concurrent > prevMax && !maxConcurrent.compare_exchange_weak(prevMax, concurrent)){}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				current.fetch_sub(1);
			});
		}

		pool.waitUntilFinished();

		test::check(maxConcurrent.load() > 1, "ranConcurrently");
		test::check(maxConcurrent.load() <= static_cast<int>(maxThreads), "neverExceedsMaxThreads");
	});

	return app.main(argc, argv);
}
