#pragma once

/**
 * @file video_stream.h
 * @brief Public API for video streaming
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace rtc
{
namespace video
{

struct VideoFrame;
struct EncodedFrame;
enum class VideoCodecType;

/**
 * @brief Video stream configuration
 */
struct VideoStreamConfig
{
  VideoCodecType codec;  // = VideoCodecType::H264
  int width = 1280;
  int height = 720;
  int fps = 30;
  int bitrate_kbps = 1500;
  bool enable_simulcast = false;
  bool use_hardware = false;
};

/**
 * @brief Video stream statistics
 */
struct VideoStreamStats
{
  uint64_t frames_sent = 0;
  uint64_t frames_received = 0;
  uint64_t frames_dropped = 0;
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;
  float packet_loss_rate = 0.0f;
  float current_bitrate_kbps = 0.0f;
  int current_width = 0;
  int current_height = 0;
  int current_fps = 0;
  float encode_time_ms = 0.0f;
  float decode_time_ms = 0.0f;
};

/**
 * @brief Callback for encoded video ready to send
 */
using VideoSendCallback = std::function<void(std::span<const uint8_t> data, uint32_t timestamp,
                                             uint16_t sequence, bool is_keyframe)>;

/**
 * @brief Callback for decoded video ready for display
 */
using VideoRenderCallback = std::function<void(const VideoFrame& frame)>;

/**
 * @brief Callback for keyframe request
 */
using KeyframeRequestCallback = std::function<void()>;

class VideoStream;

/**
 * @brief Create a video stream
 * @param config Stream configuration
 * @return Video stream instance
 */
std::unique_ptr<VideoStream> create_video_stream(VideoStreamConfig config = {});

/**
 * @brief Video stream for sending and receiving video
 */
class VideoStream
{
 public:
  virtual ~VideoStream() = default;

  /**
   * @brief Start the video stream
   * @return True if successful
   */
  virtual bool start() = 0;

  /**
   * @brief Stop the video stream
   */
  virtual void stop() = 0;

  /**
   * @brief Set callback for encoded video packets
   */
  virtual void set_send_callback(VideoSendCallback callback) = 0;

  /**
   * @brief Set callback for decoded video frames
   */
  virtual void set_render_callback(VideoRenderCallback callback) = 0;

  /**
   * @brief Set callback for keyframe requests
   */
  virtual void set_keyframe_request_callback(KeyframeRequestCallback callback) = 0;

  /**
   * @brief Receive an encoded video packet
   * @param data Encoded frame data
   * @param timestamp RTP timestamp
   * @param sequence RTP sequence number
   * @param marker RTP marker bit
   */
  virtual void receive_packet(std::span<const uint8_t> data, uint32_t timestamp, uint16_t sequence,
                              bool marker) = 0;

  /**
   * @brief Force keyframe generation
   */
  virtual void request_keyframe() = 0;

  /**
   * @brief Update target bitrate (from REMB)
   * @param bitrate_kbps New bitrate in kbps
   */
  virtual void set_target_bitrate(int bitrate_kbps) = 0;

  /**
   * @brief Get current statistics
   */
  virtual VideoStreamStats stats() const = 0;

  /**
   * @brief Enable/disable video
   */
  virtual void set_enabled(bool enabled) = 0;

  /**
   * @brief Check if enabled
   */
  virtual bool is_enabled() const = 0;

 protected:
  VideoStream() = default;
};

}  // namespace video
}  // namespace rtc
