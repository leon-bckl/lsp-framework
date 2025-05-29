#include <lsp/io/socket.h>

#ifndef LSP_SOCKET_UNSUPPORTED

#include <cassert>

#ifdef LSP_SOCKET_POSIX
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace lsp::io{

struct Socket::Impl{
#ifdef LSP_SOCKET_POSIX
	int            m_socketFd       = -1;
	unsigned short m_maxConnections = 1; // Only relevant for listen

	Impl(int socket, unsigned short maxConnections = 1)
		: m_socketFd(socket)
		, m_maxConnections(maxConnections)
	{
	}

	~Impl()
	{
		if(m_socketFd != -1)
		{
			close(m_socketFd);
			m_socketFd = -1;
		}
	}

	[[nodiscard]]
	static std::unique_ptr<Impl> setupForListen(unsigned short port, unsigned short maxConnections)
	{
		const auto socketFd = socket(AF_INET, SOCK_STREAM, 0);

		if(socketFd == -1)
			throw Error(std::string("socket: ") + strerror(errno));

		auto addr = sockaddr_in{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port        = htons(port);

		if(bind(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
			throw Error(std::string("bind: ") + strerror(errno));

		return std::make_unique<Impl>(socketFd, maxConnections);
	}

	[[nodiscard]]
	static std::unique_ptr<Impl> connect(const std::string& address, unsigned short port)
	{
		addrinfo hints = {};
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		addrinfo* addrInfoList = nullptr;

		if(auto status = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &addrInfoList); status != 0)
			throw Error(std::string("getaddrinfo: ") + gai_strerror(status));

		// Always just use the first result
		const auto socketFd = socket(addrInfoList->ai_family, addrInfoList->ai_socktype, addrInfoList->ai_protocol);

		if(socketFd == -1)
		{
			freeaddrinfo(addrInfoList);
			throw Error(std::string("socket: ") + strerror(errno));
		}

		if(::connect(socketFd, addrInfoList->ai_addr, addrInfoList->ai_addrlen) != 0)
		{
			freeaddrinfo(addrInfoList);
			throw Error(std::string("connect: ") + strerror(errno));
		}

		freeaddrinfo(addrInfoList);

		return std::make_unique<Impl>(socketFd);
	}

	std::unique_ptr<Impl> listen()
	{
		assert(m_socketFd != -1);

		if(::listen(m_socketFd, m_maxConnections) == -1)
			throw Error(std::string("listen: ") + strerror(errno));

		sockaddr  sockAddr;
		socklen_t sockAddrLen = 0;
		auto other = accept(m_socketFd, &sockAddr, &sockAddrLen);

		if(other == -1)
			throw Error(std::string("accept: ") + strerror(errno));

		return std::make_unique<Impl>(other);
	}

	void read(char* buffer, std::size_t size)
	{
		std::size_t totalBytesRead = 0;

		while(totalBytesRead < size)
		{
			const auto bytesRead = ::read(m_socketFd, buffer + totalBytesRead, size - totalBytesRead);

			if(bytesRead < 0)
			{
				if(errno == EINTR)
					continue;

				throw Error(std::string("Failed to read from socket: ") + strerror(errno));
			}

			totalBytesRead += bytesRead;
		}
	}

	void write(const char* buffer, std::size_t size)
	{
		std::size_t totalBytesWritten = 0;

		while(totalBytesWritten < size)
		{
			const auto bytesWritten = ::write(m_socketFd, buffer + totalBytesWritten, size - totalBytesWritten);

			if(bytesWritten < 0)
			{
				if(errno == EINTR)
					continue;

				throw Error(std::string("Failed to write to socket: ") + strerror(errno));
			}

			totalBytesWritten += bytesWritten;
		}
	}
#else
#error Missing implementation or #define
#endif
};

/*
 * Socket
 */

Socket::~Socket() = default;
Socket::Socket(Socket&&) = default;
Socket& Socket::operator=(Socket&&) = default;

Socket::Socket(std::unique_ptr<Impl> impl)
	: m_impl(std::move(impl))
{
}

Socket Socket::connect(const std::string& address, unsigned short port)
{
	return Socket(Impl::connect(address, port));
}


bool Socket::isOpen() const
{
	return !!m_impl;
}

void Socket::read(char* buffer, std::size_t size)
{
	assert(m_impl);
	m_impl->read(buffer, size);
}

void Socket::write(const char* buffer, std::size_t size)
{
	assert(m_impl);
	m_impl->write(buffer, size);
}

/*
 * SocketServer
 */

SocketServer::SocketServer(unsigned short port, unsigned short maxConnections)
	: m_socket(Socket::Impl::setupForListen(port, maxConnections))
{
}

Socket SocketServer::listen()
{
	if(!isReady())
		throw Error("Server socket is not open for listening");

	return Socket(m_socket.m_impl->listen());
}

} // namespace lsp::io

#endif // LSP_SOCKET_UNSUPPORTED
