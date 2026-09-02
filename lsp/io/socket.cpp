#include <lsp/io/socket.h>

#ifndef LSP_SOCKET_UNSUPPORTED

#include <cassert>

#ifdef LSP_SOCKET_POSIX
#include <cerrno>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined LSP_SOCKET_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <mutex> // std::call_once
#endif

namespace lsp::io{

/*
 * Socket::Impl
 */

struct Socket::Impl : Stream{
	static constexpr auto InvalidSocket =
#ifdef LSP_SOCKET_POSIX
	-1;
#elif defined(LSP_SOCKET_WIN32)
	INVALID_SOCKET;
#endif
	using SocketHandle =
#ifdef LSP_SOCKET_POSIX
	int;
#elif defined(LSP_SOCKET_WIN32)
	SOCKET;
#endif

	using SizeType =
#ifdef LSP_SOCKET_POSIX
		socklen_t;
#elif defined(LSP_SOCKET_WIN32)
		int;
#endif

	SocketHandle   m_socketFd = InvalidSocket;
	unsigned short m_port     = 0;
	unsigned short m_backlog  = 1; // Only relevant for listen

	Impl(SocketHandle socket, unsigned short port, unsigned short backlog = 1)
		: m_socketFd(socket)
		, m_port(port)
		, m_backlog(backlog)
	{
	}

	~Impl()
	{
		closeSocketHandle(m_socketFd);
	}

	static void closeSocketHandle(SocketHandle handle)
	{
		if(handle == InvalidSocket)
			return;

#ifdef LSP_SOCKET_POSIX
		shutdown(handle, SHUT_RDWR);
		::close(handle);
#elif defined(LSP_SOCKET_WIN32)
		shutdown(handle, SD_BOTH);
		closesocket(handle);
#endif
	}

	static void ensureInitialized()
	{
#ifdef LSP_SOCKET_WIN32
		static std::once_flag initFlag;
		std::call_once(initFlag, []()
		{
				WSADATA wsaData;
				if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
					throwError("WSAStartup failed");
		});
#endif
	}

	[[noreturn]]
	static void throwError(const std::string& msg)
	{
		const auto errorCode =
#ifdef LSP_SOCKET_POSIX
		errno;
#elif defined(LSP_SOCKET_WIN32)
		WSAGetLastError();
#endif
		throw Error(msg + ": " + std::to_string(errorCode));
	}

	static auto setupForListen(unsigned short port, unsigned short backlog) -> std::unique_ptr<Impl>
	{
		ensureInitialized();

		const auto socketFd = socket(AF_INET, SOCK_STREAM, 0);

		if(socketFd == InvalidSocket)
			throwError("Failed to create socket");

#ifdef LSP_SOCKET_POSIX
		const int yes     = 1;
		const int optName = SO_REUSEADDR;
#elif defined(LSP_SOCKET_WIN32)
		const char yes    = 1;
		const int optName = SO_EXCLUSIVEADDRUSE;
#endif
		setsockopt(socketFd, SOL_SOCKET, optName, &yes, sizeof(yes));

		auto addr = sockaddr_in{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port        = htons(port);

		if(bind(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
		{
			closeSocketHandle(socketFd);
			throwError("Failed to bind socket address");
		}

		SizeType addrLen = sizeof(addr);
		if(getsockname(socketFd, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0)
		{
			closeSocketHandle(socketFd);
			throwError("Failed to get listen port");
		}

		return std::make_unique<Impl>(socketFd, ntohs(addr.sin_port), backlog);
	}

	static auto connect(const std::string& address, unsigned short port) -> std::unique_ptr<Impl>
	{
		if(port == 0)
			throw Error("Cannot connect on port 0");

		ensureInitialized();

		addrinfo hints = {};
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		addrinfo* addrInfoList = nullptr;

		if(auto status = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &addrInfoList); status != 0)
			throwError("getaddrinfo: " + std::to_string(status));

		for(const auto* addr = addrInfoList; addr; addr = addr->ai_next)
		{
			const auto socketFd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

			if(socketFd == InvalidSocket)
				continue;

			if(::connect(socketFd, addr->ai_addr, static_cast<SizeType>(addr->ai_addrlen)) == 0)
			{
				freeaddrinfo(addrInfoList);
				return std::make_unique<Impl>(socketFd, port);
			}

			closeSocketHandle(socketFd);
		}

		freeaddrinfo(addrInfoList);

		throwError("Failed to connect to any resolved address");
	}

	auto accept() -> std::unique_ptr<Impl>
	{
		assert(m_socketFd != InvalidSocket);

		if(::listen(m_socketFd, m_backlog) == -1)
			throwError("Failed to listen for new socket connections");

		const auto other = ::accept(m_socketFd, nullptr, nullptr);

		if(other == InvalidSocket)
			throwError("Failed to accept socket connection");

		return std::make_unique<Impl>(other, m_port);
	}

	void read(char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		SizeType totalBytesRead = 0;

		while(totalBytesRead < static_cast<SizeType>(size))
		{
			const auto bytesRead = recv(m_socketFd, buffer + totalBytesRead, static_cast<SizeType>(size) - totalBytesRead, 0);
#ifdef LSP_SOCKET_POSIX
			if(bytesRead < 0 && errno == EINTR)
				continue;
#endif
			if(bytesRead <= 0)
				throwError("Failed to read from socket");

			totalBytesRead += bytesRead;
		}
	}

	void write(const char* buffer, std::size_t size) override
	{
		if(size == 0)
			return;

		SizeType totalBytesWritten = 0;

		while(totalBytesWritten < static_cast<SizeType>(size))
		{
			const auto bytesWritten = send(m_socketFd, buffer + totalBytesWritten, static_cast<SizeType>(size) - totalBytesWritten, 0);
#ifdef LSP_SOCKET_POSIX
			if(bytesWritten < 0 && errno == EINTR)
				continue;
#endif
			if(bytesWritten <= 0)
				throwError("Failed to write to socket");

			totalBytesWritten += bytesWritten;
		}
	}
};

/*
 * Socket
 */

Socket::~Socket() = default;
Socket::Socket(Socket&&) noexcept = default;
Socket& Socket::operator=(Socket&&) noexcept = default;

Socket::Socket(std::unique_ptr<Impl> impl)
	: m_impl(std::move(impl))
{
}

Socket Socket::connect(const std::string& address, unsigned short port)
{
	return Socket(Impl::connect(address, port));
}

auto Socket::isOpen() const -> bool
{
	return !!m_impl;
}

auto Socket::port() const -> unsigned short
{
	if(m_impl)
		return m_impl->m_port;

	return 0;
}

void Socket::close()
{
	if(m_impl)
		m_impl.reset();
}

void Socket::read(char* buffer, std::size_t size)
{
	if(!m_impl)
		throw Error("Socket is not open for reading");

	m_impl->read(buffer, size);
}

void Socket::write(const char* buffer, std::size_t size)
{
	if(!m_impl)
		throw Error("Socket is not open for writing");

	m_impl->write(buffer, size);
}

auto Socket::stream() -> Stream&
{
	assert(m_impl);
	return *m_impl;
}

/*
 * SocketListener
 */

SocketListener::SocketListener(unsigned short port, unsigned short backlog)
	: m_socket(Socket::Impl::setupForListen(port, backlog))
{
}

auto SocketListener::accept() -> Socket
{
	if(!isOpen())
		throw Error("Server socket is not open for listening");

	return Socket(m_socket.m_impl->accept());
}

} // namespace lsp::io

#endif // LSP_SOCKET_UNSUPPORTED
