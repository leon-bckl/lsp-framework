#include <lsp/threadpool.h>

namespace lsp{

ThreadPool::ThreadPool(unsigned int initialThreads, unsigned int maxThreads)
	: m_maxThreads{std::max(maxThreads, 1u)}
{
	const auto lock = std::lock_guard(m_mutex);
	m_threads.reserve(initialThreads);
	m_waitForNewTasks = true;

	for(std::size_t i = 0; i < initialThreads; ++i)
		addThread();
}

ThreadPool::~ThreadPool()
{
	waitUntilFinished();
}

void ThreadPool::waitUntilFinished()
{
	{
		const auto lock = std::lock_guard(m_mutex);
		m_waitForNewTasks = false;
	}

	m_event.notify_all();

	for(auto& t : m_threads)
		t.join();

	{
		const auto lock = std::lock_guard(m_mutex);
		m_threads.clear();
		m_waitForNewTasks = true;
	}

	// Notify threads waiting in addTask
	m_event.notify_all();
}

void ThreadPool::addTask(TaskPtr task)
{
	auto lock = std::unique_lock(m_mutex);

	if(!m_waitForNewTasks)
		m_event.wait(lock, [this](){ return m_waitForNewTasks; });

	m_taskQueue.emplace(std::move(task));

	if((m_taskQueue.size() > 1 && m_threads.size() < m_maxThreads) || m_threads.empty())
		addThread();

	lock.unlock();
	m_event.notify_one();
}

void ThreadPool::addThread()
{
	m_threads.emplace_back([this]()
	{
		while(true)
		{
			TaskPtr task;

			{
				auto lock = std::unique_lock(m_mutex);

				if(m_waitForNewTasks && m_taskQueue.empty())
					m_event.wait(lock, [this](){ return !m_waitForNewTasks || !m_taskQueue.empty(); });

				if(!m_taskQueue.empty())
				{
					task = std::move(m_taskQueue.front());
					m_taskQueue.pop();
				}
			}

			if(!task) // No more tasks in the queue. Thread was notified to exit.
				break;

			task->execute();
		}
	});
}

} // namespace lsp
