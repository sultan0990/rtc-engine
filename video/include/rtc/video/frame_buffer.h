#pragma once

/**
 * @file frame_buffer.h
 * @brief Frame reordering and jitter buffer for video
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace rtc
{
namespace video
{

struct EncodedFrame;
struct VideoFrame;

/**
 * @brief Buffered frame with metadata
 */
struct BufferedFrame
{
  std::vector<uint8_t> data;
  uint32_t rtp_timestamp = 0;
  uint16_t sequence_start = 0;
  uint16_t sequence_end = 0;
  std::chrono::steady_clock::time_point arrival_time;
  bool is_keyframe = false;
  bool is_complete = false;  // All packets received
};

/**
 * @brief Frame buffer statistics
 */
struct FrameBufferStats
{
  size_t frames_buffered = 0;
  size_t frames_decoded = 0;
  size_t frames_dropped = 0;
  size_t packets_lost = 0;
  float packet_loss_rate = 0.0f;
  float current_delay_ms = 0.0f;
};

/**
 * @brief Frame buffer configuration
 */
struct FrameBufferConfig
{
  size_t max_frames = 30;                      // Maximum frames to buffer
  std::chrono::milliseconds max_delay{200};    // Max playout delay
  std::chrono::milliseconds target_delay{50};  // Target delay
  bool enable_nack = true;                     // Request retransmission
  bool wait_for_keyframe = true;               // Wait for keyframe on start
};

/**
 * @brief Frame reordering buffer for video
 *
 * Handles:
 * - RTP packet reassembly into frames
 * - Frame reordering by timestamp
 * - Keyframe detection
 * - NACK generation for lost packets
 */
class FrameBuffer
{
 public:
  explicit FrameBuffer(FrameBufferConfig config = {});
  ~FrameBuffer();

  // Disable copy
  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  /**
   * @brief Insert an RTP packet
   * @param data Packet payload (without RTP header)
   * @param sequence RTP sequence number
   * @param timestamp RTP timestamp
   * @param marker Marker bit (end of frame)
   * @param is_keyframe_packet Keyframe indicator
   */
  void insert_packet(std::span<const uint8_t> data, uint16_t sequence, uint32_t timestamp,
                     bool marker, bool is_keyframe_packet);

  /**
   * @brief Get next complete frame for decoding
   * @return Complete frame or nullopt if not ready
   */
  std::optional<BufferedFrame> pop_frame();

  /**
   * @brief Peek at next frame without removing
   */
  std::optional<BufferedFrame> peek_frame() const;

  /**
   * @brief Check if a complete frame is ready
   */
  [[nodiscard]] bool has_complete_frame() const;

  /**
   * @brief Get list of lost sequence numbers for NACK
   * @param max_count Maximum number to return
   * @return Lost sequence numbers
   */
  [[nodiscard]] std::vector<uint16_t> get_nack_list(size_t max_count = 10) const;

  /**
   * @brief Request keyframe (when too many frames lost)
   */
  [[nodiscard]] bool should_request_keyframe() const;

  /**
   * @brief Get current statistics
   */
  [[nodiscard]] FrameBufferStats stats() const;

  /**
   * @brief Reset the buffer
   */
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace video
}  // namespace rtc
