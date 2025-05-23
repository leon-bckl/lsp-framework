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

namespace lsp{
namespace io{

class Stream;

} // namespace io

/*
 * Error
 */

class ProcessError : public std::runtime_error{
public:
	using std::runtime_error::runtime_error;
};

/*
 * Process
 */

class Process{
public:
	using ArgList = std::vector<std::string>;

	Process() = default;
	Process(const std::string& executable, const ArgList& args = {});
	~Process();

	void start(const std::string& executable, const ArgList& args = {});
	[[nodiscard]] bool isRunning() const;
	void wait();
	void terminate();
	[[nodiscard]] io::Stream& stdIO();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace lsp

#endif // LSP_PROCESS_UNSUPPORTED
