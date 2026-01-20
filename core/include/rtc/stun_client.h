#pragma once

/**
 * @file stun_client.h
 * @brief STUN (Session Traversal Utilities for NAT) client
 *
 * Implements RFC 5389 STUN protocol for NAT traversal.
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
 * @brief STUN message types
 */
enum class StunMessageType : uint16_t
{
  BINDING_REQUEST = 0x0001,
  BINDING_RESPONSE = 0x0101,
  BINDING_ERROR_RESPONSE = 0x0111,
  BINDING_INDICATION = 0x0011,
};

/**
 * @brief STUN attribute types
 */
enum class StunAttributeType : uint16_t
{
  MAPPED_ADDRESS = 0x0001,
  USERNAME = 0x0006,
  MESSAGE_INTEGRITY = 0x0008,
  ERROR_CODE = 0x0009,
  UNKNOWN_ATTRIBUTES = 0x000A,
  REALM = 0x0014,
  NONCE = 0x0015,
  XOR_MAPPED_ADDRESS = 0x0020,
  SOFTWARE = 0x8022,
  FINGERPRINT = 0x8028,
  PRIORITY = 0x0024,
  USE_CANDIDATE = 0x0025,
  ICE_CONTROLLED = 0x8029,
  ICE_CONTROLLING = 0x802A,
};

/**
 * @brief STUN transaction ID (96 bits)
 */
struct StunTransactionId
{
  uint8_t data[12];

  bool operator==(const StunTransactionId& other) const;
  [[nodiscard]] static StunTransactionId generate();
};

/**
 * @brief STUN attribute base
 */
struct StunAttribute
{
  StunAttributeType type;
  std::vector<uint8_t> value;
};

/**
 * @brief STUN message
 */
class StunMessage
{
 public:
  StunMessage() = default;
  explicit StunMessage(StunMessageType type);

  /**
   * @brief Parse STUN message from raw data
   */
  [[nodiscard]] static std::optional<StunMessage> parse(std::span<const uint8_t> data);

  /**
   * @brief Serialize message to bytes
   */
  [[nodiscard]] std::vector<uint8_t> serialize() const;

  /**
   * @brief Add message integrity (HMAC-SHA1)
   */
  void add_message_integrity(std::string_view password);

  /**
   * @brief Verify message integrity
   */
  [[nodiscard]] bool verify_message_integrity(std::string_view password) const;

  /**
   * @brief Add fingerprint (CRC32)
   */
  void add_fingerprint();

  /**
   * @brief Get XOR-MAPPED-ADDRESS if present
   */
  [[nodiscard]] std::optional<SocketAddress> get_xor_mapped_address() const;

  // Accessors
  [[nodiscard]] StunMessageType type() const
  {
    return type_;
  }
  [[nodiscard]] const StunTransactionId& transaction_id() const
  {
    return transaction_id_;
  }
  [[nodiscard]] const std::vector<StunAttribute>& attributes() const
  {
    return attributes_;
  }

  void set_type(StunMessageType type)
  {
    type_ = type;
  }
  void set_transaction_id(const StunTransactionId& id)
  {
    transaction_id_ = id;
  }
  void add_attribute(StunAttribute attr);

 private:
  StunMessageType type_ = StunMessageType::BINDING_REQUEST;
  StunTransactionId transaction_id_{};
  std::vector<StunAttribute> attributes_;
};

/**
 * @brief Result of STUN binding request
 */
struct StunResult
{
  bool success = false;
  SocketAddress reflexive_address;  // Server-reflexive address
  std::string error_message;
  std::chrono::milliseconds rtt{0};
};

/**
 * @brief Callback for async STUN operations
 */
using StunCallback = std::function<void(StunResult)>;

/**
 * @brief STUN client for discovering public IP
 */
class StunClient
{
 public:
  /**
   * @brief Configuration
   */
  struct Config
  {
    std::vector<std::string> servers = {
        "stun.l.google.com:19302",
        "stun1.l.google.com:19302",
    };
    std::chrono::milliseconds timeout{3000};
    int max_retries = 3;
  };

  explicit StunClient(std::shared_ptr<UdpSocket> socket, Config config = {});
  ~StunClient();

  /**
   * @brief Send binding request and get reflexive address
   * @param callback Called with result
   */
  void get_reflexive_address(StunCallback callback);

  /**
   * @brief Send binding request (synchronous)
   * @return Result with reflexive address
   */
  [[nodiscard]] StunResult get_reflexive_address_sync();

  /**
   * @brief Process incoming STUN response
   * @param data Raw packet data
   * @param source Source address
   * @return True if packet was a STUN message
   */
  bool process_packet(std::span<const uint8_t> data, const SocketAddress& source);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
