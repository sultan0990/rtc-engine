#pragma once

/**
 * @file rtp_forwarder.h
 * @brief Zero-copy RTP packet forwarding for SFU
 *
 * Core component of the Selective Forwarding Unit.
 * Forwards RTP packets from publishers to subscribers
 * without transcoding.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtc
{

struct SocketAddress;
class UdpSocket;

namespace server
{

/**
 * @brief Participant identifier
 */
using ParticipantId = std::string;

/**
 * @brief Stream identifier (one participant may have multiple streams)
 */
using StreamId = std::string;

/**
 * @brief RTP stream info
 */
struct RtpStreamInfo
{
  uint32_t ssrc = 0;
  uint8_t payload_type = 0;
  bool is_audio = false;
  int simulcast_layer = -1;  // -1 if not simulcast, 0-2 for layers
  std::string codec_name;    // "opus", "h264", "vp8"
};

/**
 * @brief Forwarding rule for a subscriber
 */
struct ForwardingRule
{
  ParticipantId subscriber_id;
  SocketAddress destination;
  uint32_t rewritten_ssrc = 0;  // SSRC to use when forwarding
  int preferred_simulcast_layer = -1;
  bool is_active = true;
};

/**
 * @brief Forwarding statistics
 */
struct ForwarderStats
{
  uint64_t packets_received = 0;
  uint64_t packets_forwarded = 0;
  uint64_t bytes_received = 0;
  uint64_t bytes_forwarded = 0;
  uint64_t packets_dropped = 0;
  size_t active_publishers = 0;
  size_t active_subscribers = 0;
};

/**
 * @brief Callback when sending forwarded packet
 */
using ForwardCallback =
    std::function<void(const ParticipantId& subscriber, std::span<const uint8_t> packet,
                       const SocketAddress& destination)>;

/**
 * @brief Zero-copy RTP packet forwarder
 *
 * Implements selective forwarding:
 * - Receives RTP packets from publishers
 * - Rewrites SSRC if needed
 * - Forwards to all subscribers
 * - Handles simulcast layer selection
 */
class RtpForwarder
{
 public:
  RtpForwarder();
  ~RtpForwarder();

  // Disable copy
  RtpForwarder(const RtpForwarder&) = delete;
  RtpForwarder& operator=(const RtpForwarder&) = delete;

  /**
   * @brief Set callback for sending forwarded packets
   */
  void set_forward_callback(ForwardCallback callback);

  /**
   * @brief Register a publisher stream
   * @param publisher_id Publisher identifier
   * @param stream_id Stream identifier
   * @param info Stream info (SSRC, codec, etc.)
   */
  void add_publisher(const ParticipantId& publisher_id, const StreamId& stream_id,
                     const RtpStreamInfo& info);

  /**
   * @brief Remove a publisher stream
   */
  void remove_publisher(const ParticipantId& publisher_id, const StreamId& stream_id);

  /**
   * @brief Add a subscription (subscriber wants to receive from publisher)
   * @param publisher_id Publisher to subscribe to
   * @param subscriber_id Subscriber identifier
   * @param rule Forwarding rule
   */
  void add_subscription(const ParticipantId& publisher_id, const ParticipantId& subscriber_id,
                        ForwardingRule rule);

  /**
   * @brief Remove a subscription
   */
  void remove_subscription(const ParticipantId& publisher_id, const ParticipantId& subscriber_id);

  /**
   * @brief Set preferred simulcast layer for a subscription
   */
  void set_simulcast_layer(const ParticipantId& publisher_id, const ParticipantId& subscriber_id,
                           int layer);

  /**
   * @brief Process an incoming RTP packet from a publisher
   * @param ssrc Source SSRC
   * @param packet Raw RTP packet
   * @param source Source address
   */
  void on_rtp_packet(uint32_t ssrc, std::span<const uint8_t> packet, const SocketAddress& source);

  /**
   * @brief Get current statistics
   */
  [[nodiscard]] ForwarderStats stats() const;

  /**
   * @brief Get list of active publishers
   */
  [[nodiscard]] std::vector<ParticipantId> get_publishers() const;

  /**
   * @brief Get subscribers for a publisher
   */
  [[nodiscard]] std::vector<ParticipantId> get_subscribers(const ParticipantId& publisher_id) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
