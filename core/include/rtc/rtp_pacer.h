#pragma once

/**
 * @file rtp_pacer.h
 * @brief RTP packet pacing using token bucket algorithm
 *
 * Smooths outgoing packet bursts to avoid network congestion.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace rtc
{

struct SocketAddress;

/**
 * @brief Queued packet for pacing
 */
struct PacedPacket
{
  std::vector<uint8_t> data;
  SocketAddress destination;
  std::chrono::steady_clock::time_point enqueue_time;
  int priority = 0;  // Higher = more important
};

/**
 * @brief Callback to send paced packet
 */
using PacerSendCallback = std::function<void(const std::vector<uint8_t>&, const SocketAddress&)>;

/**
 * @brief Token bucket RTP pacer
 *
 * Implements a token bucket algorithm to smooth packet transmission.
 * - Tokens are added at a steady rate (target bitrate / 8)
 * - Each packet consumes tokens equal to its size
 * - Packets are queued if insufficient tokens available
 * - Supports priority queuing (audio > video)
 */
class RtpPacer
{
 public:
  /**
   * @brief Configuration for the pacer
   */
  struct Config
  {
    uint64_t target_bitrate_bps = 1'000'000;       // 1 Mbps default
    uint64_t max_bitrate_bps = 2'000'000;          // 2 Mbps max
    size_t bucket_size_bytes = 10'000;             // Token bucket capacity
    size_t max_queue_size = 1000;                  // Max packets in queue
    std::chrono::milliseconds pacing_interval{5};  // Pacing interval
  };

  explicit RtpPacer(Config config = {});
  ~RtpPacer();

  // Disable copy
  RtpPacer(const RtpPacer&) = delete;
  RtpPacer& operator=(const RtpPacer&) = delete;

  /**
   * @brief Set the send callback
   * @param callback Function called when packet should be sent
   */
  void set_send_callback(PacerSendCallback callback);

  /**
   * @brief Queue a packet for paced sending
   * @param data Packet data
   * @param destination Destination address
   * @param priority Packet priority (audio=10, video=5, fec=1)
   * @return True if queued, false if queue is full
   */
  bool enqueue(std::vector<uint8_t> data, const SocketAddress& destination, int priority = 0);

  /**
   * @brief Process queued packets (call periodically)
   * @return Number of packets sent
   */
  size_t process();

  /**
   * @brief Update target bitrate
   * @param bitrate_bps New target bitrate in bits per second
   */
  void set_target_bitrate(uint64_t bitrate_bps);

  /**
   * @brief Get current target bitrate
   * @return Bitrate in bits per second
   */
  [[nodiscard]] uint64_t target_bitrate() const;

  /**
   * @brief Get current queue size
   * @return Number of packets in queue
   */
  [[nodiscard]] size_t queue_size() const;

  /**
   * @brief Get current queue delay
   * @return Delay for oldest packet in queue
   */
  [[nodiscard]] std::chrono::milliseconds queue_delay() const;

  /**
   * @brief Clear all queued packets
   */
  void clear();

  /**
   * @brief Get statistics
   */
  struct Stats
  {
    uint64_t packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t packets_dropped = 0;
    std::chrono::milliseconds avg_queue_delay{0};
  };
  [[nodiscard]] Stats stats() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
