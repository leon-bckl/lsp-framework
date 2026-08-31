#pragma once

#if defined(__APPLE__) || defined(__linux__) || defined(__HAIKU__)
	#define LSP_SOCKET_POSIX
#elif defined(_WIN32)
	#define LSP_SOCKET_WIN32
#else
	#define LSP_SOCKET_UNSUPPORTED
#endif

#ifndef LSP_SOCKET_UNSUPPORTED

#include <memory>
#include <string>
#include <lsp/io/stream.h>

namespace lsp::io{

/*
 * Socket
 */

class Socket{
public:
	static constexpr auto Localhost = "127.0.0.1";

	Socket(Socket&&) noexcept;
	Socket& operator=(Socket&&) noexcept;
	~Socket();

	[[nodiscard]] static auto connect(const std::string& address, unsigned short port) -> Socket;

	[[nodiscard]] auto isOpen() const -> bool;
	[[nodiscard]] auto port() const -> unsigned short;

	void close();
	void read(char* buffer, std::size_t size);
	void write(const char* buffer, std::size_t size);

	[[nodiscard]] auto stream() -> Stream&;
	operator Stream&(){ return stream(); }

private:
	friend class SocketListener;
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	Socket(std::unique_ptr<Impl> impl);
};

/*
 * SocketListener
 */

class SocketListener{
public:
	SocketListener(unsigned short port, unsigned short backlog = 32);

	[[nodiscard]] auto accept() -> Socket;
	[[nodiscard]] auto isOpen() const -> bool{ return m_socket.isOpen(); }
	[[nodiscard]] auto port() const -> unsigned short{ return m_socket.port(); }

	void close(){ m_socket.close(); }

private:
	Socket m_socket;
};

} // namespace lsp::io

#endif // LSP_SOCKET_UNSUPPORTED
