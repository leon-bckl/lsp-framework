#pragma once

#if defined(__APPLE__) || defined(__linux__)
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

class Socket : public Stream{
public:
	static constexpr auto Localhost = "127.0.0.1";

	Socket(Socket&&);
	Socket& operator=(Socket&&);
	~Socket() override;

	[[nodiscard]] static Socket connect(const std::string& address, unsigned short port);

	[[nodiscard]] bool isOpen() const;
	void close();

	void read(char* buffer, std::size_t size) override;
	void write(const char* buffer, std::size_t size) override;

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
	SocketListener(unsigned short port, unsigned short maxConnections = 32);

	[[nodiscard]] Socket listen();
	[[nodiscard]] bool isReady() const{ return m_socket.isOpen(); }
	void shutdown(){ m_socket.close(); }

private:
	Socket m_socket;
};

} // namespace lsp::io

#endif // LSP_SOCKET_UNSUPPORTED
