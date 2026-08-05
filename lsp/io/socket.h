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

class Socket : public Stream{
public:
	static constexpr auto Localhost = "127.0.0.1";

	Socket(Socket&&) noexcept;
	Socket& operator=(Socket&&) noexcept;
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
	/*
	 * Pass 0 as the port to have the operating system choose a free one, then
	 * ask port() which it was. That is the only way to take a port without a
	 * race: choosing a number first and binding it afterwards leaves a window in
	 * which something else can take it.
	 */
	SocketListener(unsigned short port, unsigned short maxConnections = 32);

	[[nodiscard]] Socket listen();
	[[nodiscard]] bool isReady() const{ return m_socket.isOpen(); }

	/*
	 * The port that was actually bound. Equal to the one passed to the
	 * constructor unless that was 0. Returns 0 if the socket is not open.
	 */
	[[nodiscard]] unsigned short port() const;

	void shutdown(){ m_socket.close(); }

private:
	Socket m_socket;
};

} // namespace lsp::io

#endif // LSP_SOCKET_UNSUPPORTED
