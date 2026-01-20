/**
 * @file udp_socket.cpp
 * @brief Cross-platform UDP socket implementation
 */

#include "rtc/udp_socket.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
using socket_t = int;
constexpr socket_t INVALID_SOCKET_VALUE = -1;
#endif

#include <atomic>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

namespace rtc
{

namespace
{

/**
 * @brief Initialize Winsock on Windows (no-op on other platforms)
 */
class WinsockInit
{
 public:
  WinsockInit()
  {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
  }

  ~WinsockInit()
  {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};

// Global Winsock initializer
static WinsockInit g_winsock_init;

/**
 * @brief Convert SocketAddress to sockaddr_in
 */
sockaddr_in to_sockaddr(const SocketAddress& addr)
{
  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(addr.port);
  inet_pton(AF_INET, addr.ip.c_str(), &sa.sin_addr);
  return sa;
}

/**
 * @brief Convert sockaddr_in to SocketAddress
 */
SocketAddress from_sockaddr(const sockaddr_in& sa)
{
  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &sa.sin_addr, ip_str, sizeof(ip_str));
  return {ip_str, ntohs(sa.sin_port)};
}

/**
 * @brief Get last socket error as std::error_code
 */
std::error_code get_socket_error()
{
#ifdef _WIN32
  return std::error_code(WSAGetLastError(), std::system_category());
#else
  return std::error_code(errno, std::system_category());
#endif
}

/**
 * @brief Close a socket handle
 */
void close_socket(socket_t sock)
{
  if (sock != INVALID_SOCKET_VALUE)
  {
#ifdef _WIN32
    closesocket(sock);
#else
    ::close(sock);
#endif
  }
}

}  // namespace

/**
 * @brief Concrete UDP socket implementation
 */
class UdpSocketImpl : public UdpSocket
{
 public:
  UdpSocketImpl() : socket_(INVALID_SOCKET_VALUE) {}

  ~UdpSocketImpl() override
  {
    close();
  }

  // Move operations
  UdpSocketImpl(UdpSocketImpl&& other) noexcept
      : socket_(other.socket_.exchange(INVALID_SOCKET_VALUE)),
        local_addr_(std::move(other.local_addr_))
  {
  }

  UdpSocketImpl& operator=(UdpSocketImpl&& other) noexcept
  {
    if (this != &other)
    {
      close();
      socket_.store(other.socket_.exchange(INVALID_SOCKET_VALUE));
      local_addr_ = std::move(other.local_addr_);
    }
    return *this;
  }

  bool initialize()
  {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_VALUE)
    {
      return false;
    }
    socket_.store(sock);
    return true;
  }

  std::error_code bind(std::string_view ip, uint16_t port) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return std::make_error_code(std::errc::bad_file_descriptor);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip.empty() || ip == "0.0.0.0")
    {
      addr.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
      std::string ip_str(ip);
      if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) != 1)
      {
        return std::make_error_code(std::errc::invalid_argument);
      }
    }

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
      return get_socket_error();
    }

    // Get actual bound address (in case port was 0)
    socklen_t addr_len = sizeof(addr);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0)
    {
      local_addr_ = from_sockaddr(addr);
    }

    return {};
  }

  SocketAddress local_address() const override
  {
    return local_addr_;
  }

  std::pair<std::error_code, size_t> send_to(std::span<const uint8_t> data,
                                             const SocketAddress& remote) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return {std::make_error_code(std::errc::bad_file_descriptor), 0};
    }

    sockaddr_in addr = to_sockaddr(remote);
    auto sent =
        ::sendto(sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    if (sent < 0)
    {
      return {get_socket_error(), 0};
    }

    return {{}, static_cast<size_t>(sent)};
  }

  void async_send_to(std::span<const uint8_t> data, const SocketAddress& remote,
                     SendCallback callback) override
  {
    // For now, just do synchronous send and invoke callback
    // TODO: Implement proper async with IOCP/epoll
    auto [error, bytes_sent] = send_to(data, remote);
    if (callback)
    {
      callback(error, bytes_sent);
    }
  }

  RecvResult recv_from(std::span<uint8_t> buffer, int timeout_ms) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return {{}, {}, std::make_error_code(std::errc::bad_file_descriptor)};
    }

    // Set timeout if specified
    if (timeout_ms >= 0)
    {
#ifdef _WIN32
      DWORD timeout = static_cast<DWORD>(timeout_ms);
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                 sizeof(timeout));
#else
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }

    sockaddr_in remote_addr{};
    socklen_t addr_len = sizeof(remote_addr);

    auto received =
        ::recvfrom(sock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                   reinterpret_cast<sockaddr*>(&remote_addr), &addr_len);

    if (received < 0)
    {
      return {{}, {}, get_socket_error()};
    }

    RecvResult result;
    result.data.assign(buffer.begin(), buffer.begin() + received);
    result.remote_address = from_sockaddr(remote_addr);
    return result;
  }

  void async_recv(RecvCallback callback) override
  {
    // TODO: Implement proper async with IOCP/epoll
    // For now, store callback for later use
    std::lock_guard lock(callback_mutex_);
    recv_callback_ = std::move(callback);
  }

  std::error_code set_recv_buffer_size(size_t size) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return std::make_error_code(std::errc::bad_file_descriptor);
    }

    int sz = static_cast<int>(size);
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&sz), sizeof(sz)) !=
        0)
    {
      return get_socket_error();
    }
    return {};
  }

  std::error_code set_send_buffer_size(size_t size) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return std::make_error_code(std::errc::bad_file_descriptor);
    }

    int sz = static_cast<int>(size);
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sz), sizeof(sz)) !=
        0)
    {
      return get_socket_error();
    }
    return {};
  }

  std::error_code set_non_blocking(bool non_blocking) override
  {
    socket_t sock = socket_.load();
    if (sock == INVALID_SOCKET_VALUE)
    {
      return std::make_error_code(std::errc::bad_file_descriptor);
    }

#ifdef _WIN32
    u_long mode = non_blocking ? 1 : 0;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0)
    {
      return get_socket_error();
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
      return get_socket_error();
    }

    if (non_blocking)
    {
      flags |= O_NONBLOCK;
    }
    else
    {
      flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock, F_SETFL, flags) < 0)
    {
      return get_socket_error();
    }
#endif
    return {};
  }

  void close() override
  {
    socket_t sock = socket_.exchange(INVALID_SOCKET_VALUE);
    close_socket(sock);
  }

  bool is_open() const override
  {
    return socket_.load() != INVALID_SOCKET_VALUE;
  }

  intptr_t native_handle() const override
  {
    return static_cast<intptr_t>(socket_.load());
  }

 private:
  std::atomic<socket_t> socket_;
  SocketAddress local_addr_;
  std::mutex callback_mutex_;
  RecvCallback recv_callback_;
};

// Factory method
std::unique_ptr<UdpSocket> UdpSocket::create()
{
  auto socket = std::make_unique<UdpSocketImpl>();
  if (!socket->initialize())
  {
    return nullptr;
  }
  return socket;
}

/**
 * @brief Event loop implementation (placeholder for full async support)
 */
class SocketEventLoopImpl : public SocketEventLoop
{
 public:
  SocketEventLoopImpl() : running_(false) {}

  ~SocketEventLoopImpl() override
  {
    stop();
  }

  std::error_code add_socket(UdpSocket& /*socket*/) override
  {
    // TODO: Implement with epoll/IOCP
    return {};
  }

  void remove_socket(UdpSocket& /*socket*/) override
  {
    // TODO: Implement with epoll/IOCP
  }

  void run() override
  {
    running_.store(true);
    while (running_.load())
    {
      poll(100);
    }
  }

  size_t poll(int /*timeout_ms*/) override
  {
    // TODO: Implement with epoll/IOCP
    return 0;
  }

  void stop() override
  {
    running_.store(false);
  }

  bool is_running() const override
  {
    return running_.load();
  }

 private:
  std::atomic<bool> running_;
};

std::unique_ptr<SocketEventLoop> SocketEventLoop::create()
{
  return std::make_unique<SocketEventLoopImpl>();
}

}  // namespace rtc
