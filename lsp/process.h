#pragma once

#if defined(__APPLE__) || defined(__linux__) || defined(__HAIKU__)
	#define LSP_PROCESS_POSIX
#elif defined(_WIN32)
	#define LSP_PROCESS_WIN32
#else
	#define LSP_PROCESS_UNSUPPORTED
#endif

#ifndef LSP_PROCESS_UNSUPPORTED

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <lsp/exception.h>

namespace lsp{
namespace io{
class Stream;
} // namespace io

/*
 * Exception thrown when a process failed to start
 */

class ProcessError : public Exception{
public:
	using Exception::Exception;
};

/*
 * Process
 */

class Process{
public:
	using ArgList = std::vector<std::string>;

	Process();
	Process(Process&&) noexcept;
	Process& operator=(Process&&) noexcept;
	~Process();

	[[nodiscard]] static auto start(const std::string& executable, const ArgList& args = {}) -> Process;

	[[nodiscard]] auto isRunning() const -> bool;
	[[nodiscard]] auto id() -> int;
	[[nodiscard]] auto stdIO() -> io::Stream&;
	[[nodiscard]] auto readAvailableStdErr() -> std::string;

	auto  wait() -> int;
	void terminate();

	[[nodiscard]] static auto currentProcessId() -> int;
	[[nodiscard]] static auto exists(int id) -> bool;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	Process(std::unique_ptr<Impl> impl);
};

} // namespace lsp

#endif // LSP_PROCESS_UNSUPPORTED
