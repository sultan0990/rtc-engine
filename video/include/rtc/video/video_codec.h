#pragma once

/**
 * @file video_codec.h
 * @brief Video codec wrapper for H.264 and VP8 encoding/decoding
 *
 * Uses FFmpeg (libavcodec) for software encoding and optional
 * hardware acceleration (VAAPI on Linux).
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rtc
{
namespace video
{

/**
 * @brief Supported video codecs
 */
enum class VideoCodecType
{
  H264,
  VP8,
  VP9,  // Future
  AV1,  // Future
};

/**
 * @brief H.264 profile
 */
enum class H264Profile
{
  BASELINE,
  MAIN,
  HIGH,
  CONSTRAINED_BASELINE,  // WebRTC default
};

/**
 * @brief Video frame with raw pixel data
 */
struct VideoFrame
{
  std::vector<uint8_t> data_y;  // Y plane (luma)
  std::vector<uint8_t> data_u;  // U plane (chroma)
  std::vector<uint8_t> data_v;  // V plane (chroma)
  int width = 0;
  int height = 0;
  int stride_y = 0;
  int stride_u = 0;
  int stride_v = 0;
  int64_t timestamp_us = 0;  // Capture timestamp in microseconds
  bool is_keyframe = false;

  [[nodiscard]] size_t size() const
  {
    return data_y.size() + data_u.size() + data_v.size();
  }
};

/**
 * @brief Encoded video packet
 */
struct EncodedFrame
{
  std::vector<uint8_t> data;  // NAL units (H.264) or VP8 frame
  int width = 0;
  int height = 0;
  int64_t timestamp_us = 0;
  bool is_keyframe = false;
  VideoCodecType codec = VideoCodecType::H264;

  [[nodiscard]] bool success() const
  {
    return !data.empty();
  }
};

/**
 * @brief Encoder configuration
 */
struct VideoEncoderConfig
{
  VideoCodecType codec = VideoCodecType::H264;
  int width = 1280;
  int height = 720;
  int fps = 30;
  int bitrate_kbps = 1500;      // Target bitrate
  int max_bitrate_kbps = 2500;  // Maximum bitrate
  int keyframe_interval = 60;   // Keyframe every N frames
  H264Profile h264_profile = H264Profile::CONSTRAINED_BASELINE;
  bool use_hardware = false;  // Use VAAPI if available
  int num_threads = 4;        // Encoding threads
};

/**
 * @brief Decoder configuration
 */
struct VideoDecoderConfig
{
  VideoCodecType codec = VideoCodecType::H264;
  bool use_hardware = false;  // Use VAAPI if available
  int num_threads = 4;
};

/**
 * @brief Video encoder (H.264/VP8)
 */
class VideoEncoder
{
 public:
  explicit VideoEncoder(VideoEncoderConfig config = {});
  ~VideoEncoder();

  // Disable copy
  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;

  // Enable move
  VideoEncoder(VideoEncoder&&) noexcept;
  VideoEncoder& operator=(VideoEncoder&&) noexcept;

  /**
   * @brief Initialize the encoder
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Encode a video frame
   * @param frame YUV420 frame to encode
   * @return Encoded frame or empty on failure
   */
  [[nodiscard]] EncodedFrame encode(const VideoFrame& frame);

  /**
   * @brief Force a keyframe on next encode
   */
  void request_keyframe();

  /**
   * @brief Update target bitrate
   * @param bitrate_kbps New bitrate in kbps
   */
  void set_bitrate(int bitrate_kbps);

  /**
   * @brief Update resolution (requires re-init)
   */
  void set_resolution(int width, int height);

  /**
   * @brief Get current configuration
   */
  [[nodiscard]] const VideoEncoderConfig& config() const;

  /**
   * @brief Check if encoder is initialized
   */
  [[nodiscard]] bool is_initialized() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Video decoder (H.264/VP8)
 */
class VideoDecoder
{
 public:
  explicit VideoDecoder(VideoDecoderConfig config = {});
  ~VideoDecoder();

  // Disable copy
  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  // Enable move
  VideoDecoder(VideoDecoder&&) noexcept;
  VideoDecoder& operator=(VideoDecoder&&) noexcept;

  /**
   * @brief Initialize the decoder
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Decode an encoded frame
   * @param encoded Encoded frame data
   * @return Decoded YUV frame or nullopt on failure
   */
  [[nodiscard]] std::optional<VideoFrame> decode(const EncodedFrame& encoded);

  /**
   * @brief Reset decoder state (call after seek or errors)
   */
  void reset();

  /**
   * @brief Check if decoder is initialized
   */
  [[nodiscard]] bool is_initialized() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace video
}  // namespace rtc
