#include "threadpool.h"

namespace lsp::util{

ThreadPool::ThreadPool(std::size_t threadCount)
{
	start(threadCount);
}

ThreadPool::~ThreadPool()
{
	stop();
}

void ThreadPool::start(std::size_t threadCount)
{
	std::lock_guard lock{m_mutex};
	m_threads.reserve(threadCount);
	m_running = true;

	for(std::size_t i = m_threads.size(); i < threadCount; ++i)
	{
		m_threads.emplace_back(
			[this]()
			{
				while(true)
				{
					TaskFunc task;

					{
						std::unique_lock lock{m_mutex};
						m_event.wait(lock, [this](){ return !m_running || !m_taskQueue.empty(); });

						if(!m_taskQueue.empty())
						{
							task = std::move(m_taskQueue.front());
							m_taskQueue.pop();
						}
					}

					if(!task)
						break;

					task();
				}
			});
	}
}

void ThreadPool::stop()
{
	{
		std::lock_guard lock{m_mutex};
		m_running = false;
	}

	m_event.notify_all();

	for(auto& t : m_threads)
		t.join();

	m_threads.clear();
}

}
