#pragma once

/**
 * @file udp_socket.h
 * @brief Cross-platform non-blocking UDP socket abstraction
 *
 * Provides a unified interface for UDP socket operations across
 * Windows (IOCP) and Linux (epoll) platforms.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rtc
{

/**
 * @brief Network address (IP + port)
 */
struct SocketAddress
{
    std::string ip;
    uint16_t port = 0;

    bool operator==(const SocketAddress& other) const
    {
        return ip == other.ip && port == other.port;
    }

    [[nodiscard]] std::string to_string() const
    {
        return ip + ":" + std::to_string(port);
    }
};

/**
 * @brief Result of a receive operation
 */
struct RecvResult
{
    std::vector<uint8_t> data;
    SocketAddress remote_address;
    std::error_code error;

    [[nodiscard]] bool success() const { return !error; }
};

/**
 * @brief Callback for async receive operations
 */
using RecvCallback = std::function<void(RecvResult)>;

/**
 * @brief Callback for async send operations
 */
using SendCallback = std::function<void(std::error_code, size_t bytes_sent)>;

/**
 * @brief Cross-platform non-blocking UDP socket
 *
 * This class provides a unified interface for UDP socket operations.
 * On Windows, it uses IOCP (I/O Completion Ports).
 * On Linux, it uses epoll for event notification.
 *
 * Usage:
 * @code
 * auto socket = UdpSocket::create();
 * if (auto err = socket->bind("0.0.0.0", 5000); err) {
 *     // Handle error
 * }
 * socket->async_recv([](RecvResult result) {
 *     if (result.success()) {
 *         // Process received data
 *     }
 * });
 * @endcode
 */
class UdpSocket
{
public:
    virtual ~UdpSocket() = default;

    // Disable copy
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Enable move
    UdpSocket(UdpSocket&&) = default;
    UdpSocket& operator=(UdpSocket&&) = default;

    /**
     * @brief Create a new UDP socket
     * @return Unique pointer to the socket, or nullptr on failure
     */
    [[nodiscard]] static std::unique_ptr<UdpSocket> create();

    /**
     * @brief Bind socket to a local address and port
     * @param ip Local IP address (e.g., "0.0.0.0" for any)
     * @param port Local port number
     * @return Error code (empty if successful)
     */
    [[nodiscard]] virtual std::error_code bind(std::string_view ip, uint16_t port) = 0;

    /**
     * @brief Get the local address the socket is bound to
     * @return Local socket address
     */
    [[nodiscard]] virtual SocketAddress local_address() const = 0;

    /**
     * @brief Send data to a remote address (synchronous)
     * @param data Data buffer to send
     * @param remote Remote address to send to
     * @return Pair of (error code, bytes sent)
     */
    [[nodiscard]] virtual std::pair<std::error_code, size_t> send_to(
        std::span<const uint8_t> data,
        const SocketAddress& remote) = 0;

    /**
     * @brief Send data asynchronously
     * @param data Data buffer to send
     * @param remote Remote address to send to
     * @param callback Callback invoked on completion
     */
    virtual void async_send_to(
        std::span<const uint8_t> data,
        const SocketAddress& remote,
        SendCallback callback) = 0;

    /**
     * @brief Receive data (synchronous, blocking)
     * @param buffer Buffer to receive into
     * @param timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return Receive result with data and remote address
     */
    [[nodiscard]] virtual RecvResult recv_from(
        std::span<uint8_t> buffer,
        int timeout_ms = -1) = 0;

    /**
     * @brief Start asynchronous receive
     * @param callback Callback invoked when data is received
     */
    virtual void async_recv(RecvCallback callback) = 0;

    /**
     * @brief Set socket option: receive buffer size
     * @param size Buffer size in bytes
     * @return Error code (empty if successful)
     */
    [[nodiscard]] virtual std::error_code set_recv_buffer_size(size_t size) = 0;

    /**
     * @brief Set socket option: send buffer size
     * @param size Buffer size in bytes
     * @return Error code (empty if successful)
     */
    [[nodiscard]] virtual std::error_code set_send_buffer_size(size_t size) = 0;

    /**
     * @brief Enable/disable non-blocking mode
     * @param non_blocking True for non-blocking, false for blocking
     * @return Error code (empty if successful)
     */
    [[nodiscard]] virtual std::error_code set_non_blocking(bool non_blocking) = 0;

    /**
     * @brief Close the socket
     */
    virtual void close() = 0;

    /**
     * @brief Check if socket is open
     * @return True if socket is open and valid
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Get the native socket handle
     * @return Native socket descriptor
     */
    [[nodiscard]] virtual intptr_t native_handle() const = 0;

protected:
    UdpSocket() = default;
};

/**
 * @brief Event loop for processing async socket operations
 *
 * Must be run in a dedicated thread to process I/O events.
 */
class SocketEventLoop
{
public:
    virtual ~SocketEventLoop() = default;

    /**
     * @brief Create platform-specific event loop
     * @return Unique pointer to event loop
     */
    [[nodiscard]] static std::unique_ptr<SocketEventLoop> create();

    /**
     * @brief Register a socket with the event loop
     * @param socket Socket to register
     * @return Error code (empty if successful)
     */
    [[nodiscard]] virtual std::error_code add_socket(UdpSocket& socket) = 0;

    /**
     * @brief Remove a socket from the event loop
     * @param socket Socket to remove
     */
    virtual void remove_socket(UdpSocket& socket) = 0;

    /**
     * @brief Run the event loop (blocking)
     * Call this from a dedicated I/O thread.
     */
    virtual void run() = 0;

    /**
     * @brief Run one iteration of the event loop
     * @param timeout_ms Timeout in milliseconds
     * @return Number of events processed
     */
    virtual size_t poll(int timeout_ms = 0) = 0;

    /**
     * @brief Stop the event loop
     */
    virtual void stop() = 0;

    /**
     * @brief Check if event loop is running
     * @return True if running
     */
    [[nodiscard]] virtual bool is_running() const = 0;

protected:
    SocketEventLoop() = default;
};

}  // namespace rtc
