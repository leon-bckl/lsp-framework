#include <lsp/process.h>
#include <lsp/io/stream.h>

#ifndef LSP_PROCESS_UNSUPPORTED

#ifdef LSP_PROCESS_POSIX
#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#elif defined(LSP_PROCESS_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace lsp{

struct Process::Impl final : public io::Stream{
#ifdef LSP_PROCESS_POSIX
	int   m_stdinWrite = -1;
	int   m_stdoutRead = -1;
	int   m_stderrRead = -1;
	int   m_exitCode   = -1;
	pid_t m_pid        = -1;

	Impl(const std::string& executable, const ArgList& args)
	{
		int inPipe[2]; // Parent writes to child (stdin)
		int outPipe[2]; // Parent reads from child (stdout)
		int stderrPipe[2]; // Parent reads from child (stderr)
		int errPipe[2]; // Used to inform parent about exec errors

		if(pipe(inPipe) == -1)
			throw ProcessError(strerror(errno));

		if(pipe(outPipe) == -1)
		{
			const auto error = errno;
			close(inPipe[0]);
			close(inPipe[1]);
			throw ProcessError(strerror(error));
		}

		if(pipe(stderrPipe) == -1)
		{
			const auto error = errno;
			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);
			throw ProcessError(strerror(error));
		}

		if(pipe(errPipe) == -1)
		{
			const auto error = errno;
			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);
			close(stderrPipe[0]);
			close(stderrPipe[1]);
			throw ProcessError(strerror(error));
		}

		auto argList = std::vector<char*>({const_cast<char*>(executable.c_str())});

		for(const auto& arg : args)
			argList.push_back(const_cast<char*>(arg.c_str()));

		argList.push_back(nullptr);

		const char* const file = executable.c_str();
		char** const      argv = argList.data();

		// fork

		m_pid = fork();

		if(m_pid == -1)
		{
			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);
			close(stderrPipe[0]);
			close(stderrPipe[1]);
			throw ProcessError(strerror(errno));
		}

		if(m_pid == 0) // Child process
		{
			dup2(inPipe[0], STDIN_FILENO);
			dup2(outPipe[1], STDOUT_FILENO);
			dup2(stderrPipe[1], STDERR_FILENO);

			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);
			close(stderrPipe[0]);
			close(stderrPipe[1]);
			close(errPipe[0]);
			fcntl(errPipe[1], F_SETFD, FD_CLOEXEC);

			execvp(file, argv);

			const auto error = errno;
			ssize_t x = ::write(errPipe[1], &error, sizeof(error));
			(void)x;

			close(errPipe[1]);

			_exit(EXIT_FAILURE);
		}
		else // Parent process
		{
			close(inPipe[0]);
			close(outPipe[1]);
			close(stderrPipe[1]);
			close(errPipe[1]);

			int error;
			const auto bytesRead = ::read(errPipe[0], &error, sizeof(error));
			close(errPipe[0]);

			if(bytesRead > 0)
			{
				close(inPipe[1]);
				close(outPipe[0]);
				close(stderrPipe[0]);
				waitpid(m_pid, nullptr, 0);
				throw ProcessError(strerror(error));
			}

			m_stdinWrite = inPipe[1];
			m_stdoutRead = outPipe[0];
			m_stderrRead = stderrPipe[0];
			fcntl(m_stderrRead, F_SETFL, O_NONBLOCK);
		}
	}

	~Impl()
	{
		if(checkRunning())
		{
			closeStdHandles();
			kill(m_pid, SIGKILL);
			waitpid(m_pid, nullptr, 0);
			m_pid = -1;
		}
	}

	void closeStdinWrite()
	{
		if(m_stdinWrite != -1)
		{
			close(m_stdinWrite);
			m_stdinWrite  = -1;
		}
	}

	void closeStdHandles()
	{
		closeStdinWrite();

		if(m_stdoutRead != -1)
		{
			close(m_stdoutRead);
			m_stdoutRead = -1;
		}

		if(m_stderrRead != -1)
		{
			close(m_stderrRead);
			m_stderrRead = -1;
		}
	}

	auto checkRunning() -> bool
	{
		if(m_pid != -1)
		{
			int status;
			const auto pid = waitpid(m_pid, &status, WNOHANG);

			if(WIFEXITED(status))
				m_exitCode = WEXITSTATUS(status);

			if(pid != 0)
			{
				m_pid = -1;
				closeStdinWrite();
			}
		}

		return m_pid != -1;
	}

	auto wait() -> int
	{
		if(checkRunning())
		{
			closeStdHandles();
			int status;
			waitpid(m_pid, &status, 0);
			m_pid = -1;

			if(WIFEXITED(status))
				m_exitCode = WEXITSTATUS(status);
			else if(WIFSIGNALED(status))
				m_exitCode = 128 + WTERMSIG(status);
		}

		return m_exitCode;
	}

	void terminate()
	{
		if(checkRunning())
		{
			closeStdHandles();
			kill(m_pid, SIGTERM);
			waitpid(m_pid, nullptr, 0);
			m_pid = -1;
		}
	}

	auto id() const -> int
	{
		return static_cast<int>(m_pid);
	}

	static auto currentProcessId() -> int
	{
		return static_cast<int>(getpid());
	}

	static auto exists(int id) -> bool
	{
		return id > 0 && kill(static_cast<pid_t>(id), 0) == 0;
	}

	void read(char* buffer, std::size_t size) override
	{
		std::size_t totalBytesRead = 0;

		while(totalBytesRead < size)
		{
			const auto bytesRead = ::read(m_stdoutRead, buffer + totalBytesRead, size - totalBytesRead);

			if(bytesRead < 0)
			{
				if(errno == EINTR)
					continue;

				throw io::Error(std::string("Failed to read from process stdout: ") + strerror(errno));
			}
			else if(bytesRead == 0)
			{
				throw io::Error(std::string("Reached EOF"));
			}

			totalBytesRead += static_cast<std::size_t>(bytesRead);
		}
	}

	auto readAvailableStdErr() -> std::string
	{
		if(m_stderrRead == -1)
			return {};

		auto result = std::string();
		char buffer[4096];

		for(;;)
		{
			const auto bytesRead = ::read(m_stderrRead, buffer, sizeof(buffer));

			if(bytesRead > 0)
			{
				result.append(buffer, static_cast<std::size_t>(bytesRead));
				continue;
			}

			if(bytesRead == 0)
				break;

			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;

			if(errno == EINTR)
				continue;

			throw io::Error(std::string("Failed to read from process stderr: ") + strerror(errno));
		}

		return result;
	}

	void write(const char* buffer, std::size_t size) override
	{
		std::size_t totalBytesWritten = 0;

		while(totalBytesWritten < size)
		{
			const auto bytesWritten = ::write(m_stdinWrite, buffer + totalBytesWritten, size - totalBytesWritten);

			if(bytesWritten < 0)
			{
				if(errno == EINTR)
					continue;

				throw io::Error(std::string("Failed to write to process stdin: ") + strerror(errno));
			}

			totalBytesWritten += static_cast<std::size_t>(bytesWritten);
		}
	}
#elif defined(LSP_PROCESS_WIN32)
	HANDLE              m_stdinRead    = nullptr;
	HANDLE              m_stdinWrite   = nullptr;
	HANDLE              m_stdoutRead   = nullptr;
	HANDLE              m_stdoutWrite  = nullptr;
	HANDLE              m_stderrRead   = nullptr;
	HANDLE              m_stderrWrite  = nullptr;
	PROCESS_INFORMATION m_processInfo  = {};
	int                 m_exitCode     = -1;

	static auto escapeArg(const std::string& arg) -> std::string
	{
		if(!arg.empty() && arg.find_first_of(" \t\n\v\\\",") == std::string::npos)
			return arg;

		std::string escaped;
		escaped.reserve(arg.size());
		escaped += '\"';

		for(auto it = arg.cbegin();; ++it)
		{
			unsigned int numBackslashes = 0;

			while(it != arg.cend() && *it == '\\')
			{
				++numBackslashes;
				++it;
			}

			if(it == arg.cend())
			{
				escaped.append(numBackslashes * 2, '\\');
				break;
			}

			if(*it == '\"')
			{
				escaped.append(numBackslashes * 2 + 1, '\\');
				escaped += '\"';
			}
			else
			{
				escaped.append(numBackslashes, '\\');
				escaped += *it;
			}
		}

		escaped += '\"';

		return escaped;
	}

	static auto buildCmdLine(const std::string& executable, const ArgList& args) -> std::wstring
	{
		std::string cmdLine = escapeArg(executable);

		for(const auto& arg : args)
			cmdLine += ' ' + escapeArg(arg);

		std::wstring wCmdLine;
		wCmdLine.resize(cmdLine.size() * 4);

		const auto len = MultiByteToWideChar(CP_UTF8, 0, cmdLine.data(), static_cast<int>(cmdLine.size()), wCmdLine.data(), static_cast<int>(wCmdLine.size()));

		if(len < 0)
			throw ProcessError("Failed to convert process command line");

		wCmdLine.resize(static_cast<std::size_t>(len));

		return wCmdLine;
	}

	Impl(const std::string& executable, const ArgList& args)
	{
		auto securityAttributes = SECURITY_ATTRIBUTES{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		if(!CreatePipe(&m_stdinRead, &m_stdinWrite, &securityAttributes, 0))
			throw ProcessError("Failed to create stdin pipe");

		SetHandleInformation(m_stdinWrite, HANDLE_FLAG_INHERIT, 0);

		if(!CreatePipe(&m_stdoutRead, &m_stdoutWrite, &securityAttributes, 0))
		{
			CloseHandle(m_stdinRead);
			CloseHandle(m_stdinWrite);

			throw ProcessError("Failed to create stdin pipe");
		}

		SetHandleInformation(m_stdoutRead, HANDLE_FLAG_INHERIT, 0);

		if(!CreatePipe(&m_stderrRead, &m_stderrWrite, &securityAttributes, 0))
		{
			CloseHandle(m_stdinRead);
			CloseHandle(m_stdinWrite);
			CloseHandle(m_stdoutRead);
			CloseHandle(m_stdoutWrite);

			throw ProcessError("Failed to create stderr pipe");
		}

		SetHandleInformation(m_stderrRead, HANDLE_FLAG_INHERIT, 0);

		auto cmdLine = buildCmdLine(executable, args);
		auto startupInfo = STARTUPINFOW{};
		startupInfo.cb         = sizeof(startupInfo);
		startupInfo.dwFlags    = STARTF_USESTDHANDLES;
		startupInfo.hStdInput  = m_stdinRead;
		startupInfo.hStdOutput = m_stdoutWrite;
		startupInfo.hStdError  = m_stderrWrite;

		if(!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &m_processInfo))
		{
			CloseHandle(m_stdinRead);
			CloseHandle(m_stdinWrite);
			CloseHandle(m_stdoutRead);
			CloseHandle(m_stdoutWrite);
			CloseHandle(m_stderrRead);
			CloseHandle(m_stderrWrite);

			throw ProcessError("Failed to start process");
		}

		CloseHandle(m_stdinRead);
		CloseHandle(m_stdoutWrite);
		m_stdinRead   = nullptr;
		m_stdoutWrite = nullptr;
	}

	~Impl()
	{
		terminate();
	}

	void closeStdHandles()
	{
		if(m_stdinRead)
		{
			CloseHandle(m_stdinRead);
			m_stdinRead = nullptr;
		}

		if(m_stdinWrite)
		{
			CloseHandle(m_stdinWrite);
			m_stdinWrite = nullptr;
		}

		if(m_stdoutRead)
		{
			CloseHandle(m_stdoutRead);
			m_stdoutRead = nullptr;
		}

		if(m_stdoutWrite)
		{
			CloseHandle(m_stdoutWrite);
			m_stdoutWrite = nullptr;
		}

		if(m_stderrRead)
		{
			CloseHandle(m_stderrRead);
			m_stderrRead = nullptr;
		}

		if(m_stderrWrite)
		{
			CloseHandle(m_stderrWrite);
			m_stderrWrite = nullptr;
		}
	}

	auto checkRunning() -> bool
	{
		if(!m_processInfo.hProcess)
			return false;

		DWORD exitCode;

		if(GetExitCodeProcess(m_processInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
			return true;

		m_exitCode = static_cast<int>(exitCode);
		CloseHandle(m_processInfo.hThread);
		CloseHandle(m_processInfo.hProcess);
		ZeroMemory(&m_processInfo, sizeof(m_processInfo));

		return false;
	}

	auto wait() -> int
	{
		if(checkRunning())
		{
			closeStdHandles();
			WaitForSingleObject(m_processInfo.hProcess, INFINITE);
			(void)checkRunning();
		}

		return m_exitCode;
	}

	void terminate()
	{
		if(checkRunning())
		{
			TerminateProcess(m_processInfo.hProcess, 0);
			WaitForSingleObject(m_processInfo.hProcess, INFINITE);
			CloseHandle(m_processInfo.hThread);
			CloseHandle(m_processInfo.hProcess);
			ZeroMemory(&m_processInfo, sizeof(m_processInfo));
			closeStdHandles();
		}
	}

	auto id() const -> int
	{
		return static_cast<int>(m_processInfo.dwProcessId);
	}

	static auto currentProcessId() -> int
	{
		return static_cast<int>(GetCurrentProcessId());
	}

	static auto exists(int id) -> bool
	{
		if(id > 0)
		{
			const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(id));

			if(handle)
			{
				DWORD exitCode;
				const auto running = GetExitCodeProcess(handle, &exitCode) && exitCode == STILL_ACTIVE;
				CloseHandle(handle);

				return running;
			}
		}

		return false;
	}

	void read(char* buffer, std::size_t size) override
	{
		std::size_t totalBytesRead = 0;

		while(totalBytesRead < size)
		{
			DWORD bytesRead;
			if(!ReadFile(m_stdoutRead, buffer + totalBytesRead, static_cast<DWORD>(size - totalBytesRead), &bytesRead, nullptr))
			{
				if(GetLastError() == ERROR_BROKEN_PIPE)
					throw io::Error("Reached EOF");

				throw io::Error(std::string("Failed to read from process stdout"));
			}

			totalBytesRead += bytesRead;
		}
	}

	auto readAvailableStdErr() -> std::string
	{
		if(!m_stderrRead)
			return {};

		DWORD bytesAvailable = 0;

		if(!PeekNamedPipe(m_stderrRead, nullptr, 0, nullptr, &bytesAvailable, nullptr))
		{
			if(GetLastError() == ERROR_BROKEN_PIPE)
				return {};

			throw io::Error("Failed to peek process stderr");
		}

		if(bytesAvailable == 0)
			return {};

		auto  result    = std::string(bytesAvailable, '\0');
		DWORD bytesRead = 0;

		if(!ReadFile(m_stderrRead, result.data(), bytesAvailable, &bytesRead, nullptr))
			throw io::Error("Failed to read from process stderr");

		result.resize(bytesRead);
		return result;
	}

	void write(const char* buffer, std::size_t size) override
	{
		std::size_t totalBytesWritten = 0;

		while(totalBytesWritten < size)
		{
			DWORD bytesWritten;
			if(!WriteFile(m_stdinWrite, buffer + totalBytesWritten, static_cast<DWORD>(size - totalBytesWritten), &bytesWritten, nullptr))
				throw io::Error(std::string("Failed to write to process stdin"));

			totalBytesWritten += bytesWritten;
		}
	}
#else
#error Missing implementation or #define
#endif
};

Process::Process() = default;
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

Process::Process(std::unique_ptr<Impl> impl)
	: m_impl{std::move(impl)}
{
}

Process Process::start(const std::string& executable, const ArgList& args)
{
	return Process(std::make_unique<Process::Impl>(executable, args));
}

Process::~Process()
{
	wait();
}

auto Process::isRunning() const -> bool
{
	return m_impl && m_impl->checkRunning();
}

auto Process::id() -> int
{
	if(!isRunning())
		return -1;

	return m_impl->id();
}

auto Process::stdIO() -> io::Stream&
{
	if(!isRunning())
		throw ProcessError("Process is not running - Cannot get stdio");

	return *m_impl;
}

auto Process::readAvailableStdErr() -> std::string
{
	if(!m_impl)
		return {};

	return m_impl->readAvailableStdErr();
}

auto Process::wait() -> int
{
	int exitCode = -1;

	if(m_impl)
	{
		exitCode = m_impl->wait();
		m_impl.reset();
	}

	return exitCode;
}

void Process::terminate()
{
	if(m_impl)
	{
		m_impl->terminate();
		m_impl.reset();
	}
}

auto Process::currentProcessId() -> int
{
	return Impl::currentProcessId();
}

auto Process::exists(int id) -> bool
{
	return Impl::exists(id);
}

} // namespace lsp

#endif // LSP_PROCESS_UNSUPPORTED
