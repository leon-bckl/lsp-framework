#include <lsp/process.h>
#include <lsp/io/stream.h>

#ifndef LSP_PROCESS_UNSUPPORTED

#ifdef LSP_PROCESS_POSIX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#elif defined(LSP_PROCESS_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace lsp{

struct Process::Impl final : public io::Stream{
#ifdef LSP_PROCESS_POSIX
	int   m_stdinWrite = -1;
	int   m_stdoutRead = -1;
	pid_t m_pid        = -1;

	Impl(const std::string& executable, const ArgList& args)
	{
		int inPipe[2]; // Parent writes to child (stdin)
		int outPipe[2]; // Parent reads from child (stdout)

		if(pipe(inPipe) == -1 || pipe(outPipe) == -1)
			throw ProcessError(strerror(errno));

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
			throw ProcessError(strerror(errno));
		}

		if(m_pid == 0) // Child process
		{
			dup2(inPipe[0], STDIN_FILENO);
			dup2(outPipe[1], STDOUT_FILENO);

			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);

			execvp(file, argv);
			perror("execvp");
			_exit(EXIT_FAILURE);
		}
		else // Parent process
		{
			close(inPipe[0]);
			close(outPipe[1]);
			m_stdinWrite = inPipe[1];
			m_stdoutRead = outPipe[0];
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

	void closeStdHandles()
	{
		if(m_stdinWrite != -1)
		{
			close(m_stdinWrite);
			m_stdinWrite  = -1;
		}

		if(m_stdoutRead != -1)
		{
			close(m_stdoutRead);
			m_stdoutRead = -1;
		}
	}

	[[nodiscard]]
	bool checkRunning()
	{
		if(m_pid != -1)
		{
			const auto pid = waitpid(m_pid, nullptr, WNOHANG);

			if(pid != 0)
			{
				m_pid = -1;
				closeStdHandles();
			}
		}

		return m_pid != -1;
	}

	void wait()
	{
		if(checkRunning())
		{
			closeStdHandles();
			waitpid(m_pid, nullptr, 0);
			m_pid = -1;
		}
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

			totalBytesRead += bytesRead;
		}
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

			totalBytesWritten += bytesWritten;
		}
	}
#elif defined(LSP_PROCESS_WIN32)
	HANDLE              m_stdinRead    = nullptr;
	HANDLE              m_stdinWrite   = nullptr;
	HANDLE              m_stdoutRead   = nullptr;
	HANDLE              m_stdoutWrite  = nullptr;
	PROCESS_INFORMATION m_processInfo  = {};

	static std::string escapeArg(const std::string& arg)
	{
		if(arg.find_first_of(" \t\n\v\\\",") == std::string::npos)
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

	static std::wstring buildCmdLine(const std::string& executable, const ArgList& args)
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

		auto cmdLine = buildCmdLine(executable, args);
		auto startupInfo = STARTUPINFOW{};
		startupInfo.dwFlags    = STARTF_USESTDHANDLES;
		startupInfo.hStdInput  = m_stdinRead;
		startupInfo.hStdOutput = m_stdoutWrite;

		if(!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &m_processInfo))
		{
			CloseHandle(m_stdinRead);
			CloseHandle(m_stdinWrite);
			CloseHandle(m_stdoutRead);
			CloseHandle(m_stdoutWrite);

			throw ProcessError("Failed to start process");
		}
	}

	~Impl()
	{
		terminate();
	}

	void closeStdHandles()
	{
		CloseHandle(m_stdinRead);
		CloseHandle(m_stdinWrite);
		CloseHandle(m_stdoutRead);
		CloseHandle(m_stdoutWrite);
		m_stdinRead   = nullptr;
		m_stdinWrite  = nullptr;
		m_stdoutRead  = nullptr;
		m_stdoutWrite = nullptr;
	}

	[[nodiscard]]
	bool checkRunning()
	{
		if(!m_processInfo.hProcess)
			return false;

		DWORD exitCode;

		if(GetExitCodeProcess(m_processInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
			return true;

		ZeroMemory(&m_processInfo, sizeof(m_processInfo));

		return false;
	}

	void wait()
	{
		if(checkRunning())
			WaitForSingleObject(m_processInfo.hProcess, INFINITE);
	}

	void terminate()
	{
		if(checkRunning())
		{
			TerminateProcess(m_processInfo.hProcess, 0);
			ZeroMemory(&m_processInfo, sizeof(m_processInfo));
			closeStdHandles();
		}
	}

	void read(char* buffer, std::size_t size) override
	{
		std::size_t totalBytesRead = 0;

		while(totalBytesRead < size)
		{
			DWORD bytesRead;
			if(!ReadFile(m_stdoutRead, buffer + totalBytesRead, static_cast<DWORD>(size - totalBytesRead), &bytesRead, nullptr))
				throw io::Error(std::string("Failed to read from process stdout"));

			totalBytesRead += bytesRead;
		}
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
Process::Process(Process&&) = default;
Process& Process::operator=(Process&&) = default;

Process::Process(const std::string& executable, const ArgList& args)
{
	*this = start(executable, args);
}

Process::Process(std::unique_ptr<Impl> impl)
	: m_impl(std::move(impl))
{
}

Process::~Process()
{
	wait();
}

Process Process::start(const std::string& executable, const ArgList& args)
{
	return Process(std::make_unique<Process::Impl>(executable, args));
}

bool Process::isRunning() const
{
	return m_impl && m_impl->checkRunning();
}

io::Stream& Process::stdIO()
{
	if(!isRunning())
		throw ProcessError("Process is not running - Cannot get stdio");

	return *m_impl;
}

void Process::wait()
{
	if(m_impl)
	{
		m_impl->wait();
		m_impl.reset();
	}
}

void Process::terminate()
{
	if(m_impl)
	{
		m_impl->terminate();
		m_impl.reset();
	}
}

} // namespace lsp

#endif // LSP_PROCESS_UNSUPPORTED
