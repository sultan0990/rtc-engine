#pragma once

/**
 * @file turn_client.h
 * @brief TURN (Traversal Using Relays around NAT) client
 *
 * Implements RFC 5766 TURN protocol for relay-based NAT traversal.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtc
{

struct SocketAddress;
class UdpSocket;

/**
 * @brief TURN allocation state
 */
enum class TurnState
{
  IDLE,
  ALLOCATING,
  ALLOCATED,
  REFRESHING,
  FAILED,
};

/**
 * @brief TURN allocation result
 */
struct TurnAllocation
{
  SocketAddress relayed_address;     // Public relay address
  SocketAddress mapped_address;      // Server-reflexive address
  std::chrono::seconds lifetime{0};  // Allocation lifetime
};

/**
 * @brief TURN allocation callback
 */
using TurnAllocateCallback =
    std::function<void(bool success, const TurnAllocation&, std::string error)>;

/**
 * @brief TURN permission callback
 */
using TurnPermissionCallback = std::function<void(bool success, std::string error)>;

/**
 * @brief TURN data callback (for relayed data)
 */
using TurnDataCallback =
    std::function<void(std::span<const uint8_t> data, const SocketAddress& peer)>;

/**
 * @brief TURN client for relay-based NAT traversal
 *
 * Used when direct connection is not possible (e.g., symmetric NAT).
 * The TURN server acts as a relay for media traffic.
 */
class TurnClient
{
 public:
  /**
   * @brief Configuration
   */
  struct Config
  {
    std::string server;    // TURN server address (e.g., "turn.example.com:3478")
    std::string username;  // Long-term credential username
    std::string password;  // Long-term credential password
    std::string realm;     // Authentication realm (optional)
    std::chrono::milliseconds timeout{5000};
    bool use_channels = true;  // Use channel binding for efficiency
  };

  explicit TurnClient(std::shared_ptr<UdpSocket> socket, Config config);
  ~TurnClient();

  /**
   * @brief Request a TURN allocation
   * @param callback Called with allocation result
   */
  void allocate(TurnAllocateCallback callback);

  /**
   * @brief Refresh the allocation (extend lifetime)
   * @param callback Called with result
   */
  void refresh(TurnAllocateCallback callback);

  /**
   * @brief Release the allocation
   */
  void deallocate();

  /**
   * @brief Create permission for a peer
   * @param peer_address Peer's address to allow
   * @param callback Called with result
   */
  void create_permission(const SocketAddress& peer_address, TurnPermissionCallback callback);

  /**
   * @brief Bind a channel to a peer for efficient data transfer
   * @param peer_address Peer's address
   * @param callback Called with result
   */
  void bind_channel(const SocketAddress& peer_address, TurnPermissionCallback callback);

  /**
   * @brief Send data through the relay
   * @param data Data to send
   * @param peer_address Peer's address
   * @return True if sent (or queued), false on error
   */
  bool send_to(std::span<const uint8_t> data, const SocketAddress& peer_address);

  /**
   * @brief Set callback for received data
   */
  void set_data_callback(TurnDataCallback callback);

  /**
   * @brief Process incoming packet (TURN response or data indication)
   * @param data Raw packet data
   * @param source Source address
   * @return True if packet was handled
   */
  bool process_packet(std::span<const uint8_t> data, const SocketAddress& source);

  /**
   * @brief Get current state
   */
  [[nodiscard]] TurnState state() const;

  /**
   * @brief Get current allocation (if any)
   */
  [[nodiscard]] std::optional<TurnAllocation> allocation() const;

  /**
   * @brief Get relayed address (shortcut)
   */
  [[nodiscard]] std::optional<SocketAddress> relayed_address() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
