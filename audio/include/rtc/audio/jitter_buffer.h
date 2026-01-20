#pragma once

/**
 * @file jitter_buffer.h
 * @brief Adaptive jitter buffer for smooth audio playout
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace rtc
{
namespace audio
{

/**
 * @brief Jitter buffer frame with metadata
 */
struct JitterFrame
{
  std::vector<uint8_t> data;     // Encoded audio data
  uint32_t timestamp = 0;        // RTP timestamp
  uint16_t sequence_number = 0;  // RTP sequence number
  std::chrono::steady_clock::time_point arrival_time;
};

/**
 * @brief Jitter buffer statistics
 */
struct JitterBufferStats
{
  size_t current_size = 0;                     // Packets in buffer
  std::chrono::milliseconds target_delay{20};  // Target playout delay
  std::chrono::milliseconds current_delay{0};  // Current playout delay
  float packet_loss_rate = 0.0f;               // Recent packet loss rate
  float jitter_ms = 0.0f;                      // Estimated jitter in ms
  uint64_t packets_received = 0;
  uint64_t packets_lost = 0;
  uint64_t packets_late = 0;
  uint64_t packets_duplicated = 0;
};

/**
 * @brief Configuration for jitter buffer
 */
struct JitterBufferConfig
{
  std::chrono::milliseconds min_delay{10};     // Minimum playout delay
  std::chrono::milliseconds max_delay{200};    // Maximum playout delay
  std::chrono::milliseconds target_delay{50};  // Target playout delay
  size_t max_packets = 100;                    // Maximum packets to buffer
  int sample_rate = 48000;                     // Sample rate for timestamp calc
  bool enable_adaptive = true;                 // Enable adaptive delay
};

/**
 * @brief Adaptive jitter buffer for RTP audio streams
 *
 * Handles:
 * - Packet reordering
 * - Adaptive playout delay
 * - Packet loss detection
 * - Statistics collection
 */
class JitterBuffer
{
 public:
  explicit JitterBuffer(JitterBufferConfig config = {});
  ~JitterBuffer();

  // Disable copy
  JitterBuffer(const JitterBuffer&) = delete;
  JitterBuffer& operator=(const JitterBuffer&) = delete;

  /**
   * @brief Push a received packet into the buffer
   * @param frame Frame with encoded audio data
   * @return True if packet was accepted
   */
  bool push(JitterFrame frame);

  /**
   * @brief Pop the next frame for playout
   * @return Next frame or nullopt if buffer empty/not ready
   */
  std::optional<JitterFrame> pop();

  /**
   * @brief Peek at the next frame without removing
   */
  std::optional<JitterFrame> peek() const;

  /**
   * @brief Check if buffer is ready for playout
   */
  [[nodiscard]] bool is_ready() const;

  /**
   * @brief Get number of packets in buffer
   */
  [[nodiscard]] size_t size() const;

  /**
   * @brief Get current statistics
   */
  [[nodiscard]] JitterBufferStats stats() const;

  /**
   * @brief Reset the buffer
   */
  void reset();

  /**
   * @brief Set target playout delay
   */
  void set_target_delay(std::chrono::milliseconds delay);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace audio
}  // namespace rtc
