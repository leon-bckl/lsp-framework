#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <lsp/task_function.h>

namespace lsp{

class ThreadPool{
public:
	using Task = TaskFunction<void>;

	ThreadPool(unsigned int initialThreads = 0, unsigned int maxThreads = std::thread::hardware_concurrency());
	~ThreadPool();

	void addTask(Task task);
	void waitUntilFinished();

private:
	bool                     m_waitForNewTasks = false;
	unsigned int             m_maxThreads      = std::thread::hardware_concurrency();
	std::vector<std::thread> m_threads;
	std::queue<Task>         m_taskQueue;
	std::mutex               m_mutex;
	std::condition_variable  m_event;

	void addThread();
};

} // namespace lsp
