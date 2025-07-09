#pragma once

#include <mutex>
#include <queue>
#include <future>
#include <thread>
#include <vector>
#include <memory>
#include <cassert>
#include <functional>
#include <condition_variable>

namespace lsp{

class ThreadPool{
public:
	ThreadPool(unsigned int initialThreads = 0, unsigned int maxThreads = std::thread::hardware_concurrency());
	~ThreadPool();

	void waitUntilFinished();

	template<typename F, typename ...Args>
	auto addTask(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
	{
		auto task   = std::make_unique<Task<F, Args...>>(std::forward<F>(f), std::forward<Args>(args)...);
		auto future = task->promise.get_future();
		addTask(std::move(task));
		return future;
	}

private:
	struct TaskBase;
	using TaskPtr = std::unique_ptr<TaskBase>;

	bool                     m_waitForNewTasks = false;
	unsigned int             m_maxThreads      = std::thread::hardware_concurrency();
	std::vector<std::thread> m_threads;
	std::queue<TaskPtr>      m_taskQueue;
	std::mutex               m_mutex;
	std::condition_variable  m_event;

	void addTask(TaskPtr task);
	void addThread();

	struct TaskBase{
		virtual ~TaskBase() = default;
		virtual void execute() = 0;
	};

	template<typename F, typename ...Args>
	struct Task : TaskBase{
		using ResultType = std::invoke_result_t<F, Args...>;

		Task(F&& f, Args&&... args)
			: callback{std::forward<F>(f)}
			, callbackArgs{std::make_tuple(std::forward<Args>(args)...)}
		{
		}

		F                                 callback;
		std::tuple<std::decay_t<Args>...> callbackArgs;
		std::promise<ResultType>          promise;

		void execute() override
		{
			std::apply([this](Args&&... args){
				try
				{
					if constexpr(std::same_as<ResultType, void>)
					{
						std::invoke(callback, std::forward<Args>(args)...);
						promise.set_value();
					}
					else
					{
						auto result = std::invoke(callback, std::forward<Args>(args)...);
						promise.set_value(std::move(result));
					}
				}
				catch(...)
				{
					promise.set_exception(std::current_exception());
				}
			}, std::move(callbackArgs));
		}
	};

};

} // namespace lsp
