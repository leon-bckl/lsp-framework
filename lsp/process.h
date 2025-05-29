#pragma once

#if defined(__APPLE__) || defined(__linux__)
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
	Process(const std::string& executable, const ArgList& args = {});
	Process(Process&&);
	Process& operator=(Process&&);
	~Process();

	[[nodiscard]] static Process start(const std::string& executable, const ArgList& args = {});

	[[nodiscard]] bool isRunning() const;
	[[nodiscard]] io::Stream& stdIO();
	void wait();
	void terminate();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	Process(std::unique_ptr<Impl> impl);
};

} // namespace lsp

#endif // LSP_PROCESS_UNSUPPORTED
