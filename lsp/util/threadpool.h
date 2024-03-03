#include <mutex>
#include <queue>
#include <future>
#include <thread>
#include <vector>
#include <functional>
#include <condition_variable>

namespace lsp::util{

class ThreadPool{
public:
	ThreadPool(std::size_t threadCount = 0);
	~ThreadPool();

	void start(std::size_t threadCount);
	void stop();

	template<typename F, typename ...Args>
	auto addTask(F f, Args... args) -> std::future<decltype(f(args...))>
	{
		using ReturnType = decltype(f(args...));

		std::lock_guard lock{m_mutex};
		auto boundFunc = std::bind(std::move(f), std::move(args)...);
		auto task = std::make_shared<std::packaged_task<ReturnType()>>(boundFunc);
		auto future = task->get_future();
		m_taskQueue.emplace([t = std::move(task)](){ (*t)(); });
		m_event.notify_one();

		return future;
	}

private:
	using TaskFunc = std::function<void()>;

	bool                     m_running = false;
	std::vector<std::thread> m_threads;
	std::queue<TaskFunc>     m_taskQueue;
	std::mutex               m_mutex;
	std::condition_variable  m_event;
};

} // namespace lsp::util
