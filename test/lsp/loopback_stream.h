#pragma once

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <lsp/io/stream.h>

namespace lsptest{

/*
 * In-memory io::Stream that feeds everything written to it straight back as
 * readable data.
 */
class LoopbackStream : public lsp::io::Stream{
public:
	std::function<void()> onRead;
	int                   readCount = 0;

	void read(char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		++readCount;

		if(onRead)
			onRead();

		auto lock = std::unique_lock(m_mutex);

		const auto hasData = [&]{ return m_buffer.size() - m_readOffset >= size; };

		m_dataAvailable.wait_for(lock, std::chrono::seconds(2), [&]{ return m_closed || hasData(); });

		if(!hasData())
		{
			if(m_closed && size == 1)
			{
				*buffer = Eof;
				return;
			}

			throw lsp::io::Error("LoopbackStream: no data available");
		}

		std::memcpy(buffer, m_buffer.data() + m_readOffset, size);
		m_readOffset += size;
	}

	void write(const char* buffer, std::size_t size) override
	{
		{
			auto lock = std::unique_lock(m_mutex);

			if(m_readOffset > 0)
			{
				m_buffer.erase(0, m_readOffset);
				m_readOffset = 0;
			}

			m_buffer.append(buffer, size);
		}

		m_dataAvailable.notify_all();
	}

	void close()
	{
		{
			auto lock = std::unique_lock(m_mutex);
			m_closed = true;
		}

		m_dataAvailable.notify_all();
	}

	[[nodiscard]] bool empty() const
	{
		auto lock = std::unique_lock(m_mutex);
		return m_readOffset >= m_buffer.size();
	}

	[[nodiscard]] std::string takeAll()
	{
		auto lock = std::unique_lock(m_mutex);
		auto result = m_buffer.substr(m_readOffset);
		m_buffer.clear();
		m_readOffset = 0;
		return result;
	}

private:
	mutable std::mutex      m_mutex;
	std::condition_variable m_dataAvailable;
	std::string             m_buffer;
	std::size_t             m_readOffset = 0;
	bool                    m_closed     = false;
};

} // namespace lsptest
