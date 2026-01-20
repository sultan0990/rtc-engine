#pragma once

/**
 * @file opus_codec.h
 * @brief Opus audio codec wrapper for RTC engine
 *
 * Provides encoding/decoding using libopus with settings
 * optimized for real-time voice communication.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rtc
{
namespace audio
{

/**
 * @brief Opus encoder configuration
 */
struct OpusEncoderConfig
{
  int sample_rate = 48000;     // 8000, 12000, 16000, 24000, or 48000
  int channels = 1;            // 1 (mono) or 2 (stereo)
  int bitrate = 32000;         // Target bitrate in bps (6000-510000)
  int frame_duration_ms = 20;  // 2.5, 5, 10, 20, 40, or 60 ms
  bool use_vbr = true;         // Variable bitrate
  bool use_dtx = true;         // Discontinuous transmission
  int complexity = 10;         // Encoding complexity (0-10)

  enum class Application
  {
    VOIP,      // Speech optimized
    AUDIO,     // Music/general audio
    LOW_DELAY  // Lowest latency
  };
  Application application = Application::VOIP;
};

/**
 * @brief Opus decoder configuration
 */
struct OpusDecoderConfig
{
  int sample_rate = 48000;
  int channels = 1;
};

/**
 * @brief Opus encoder result
 */
struct EncodeResult
{
  std::vector<uint8_t> data;    // Encoded Opus packet
  int samples_encoded = 0;      // Number of samples encoded
  bool voice_activity = false;  // Voice activity detected (DTX)

  [[nodiscard]] bool success() const
  {
    return !data.empty();
  }
};

/**
 * @brief Opus decoder result
 */
struct DecodeResult
{
  std::vector<int16_t> samples;  // Decoded PCM samples (interleaved if stereo)
  int samples_decoded = 0;

  [[nodiscard]] bool success() const
  {
    return samples_decoded > 0;
  }
};

/**
 * @brief Opus audio encoder
 *
 * Encodes raw PCM audio to Opus format for transmission.
 */
class OpusEncoder
{
 public:
  explicit OpusEncoder(OpusEncoderConfig config = {});
  ~OpusEncoder();

  // Disable copy
  OpusEncoder(const OpusEncoder&) = delete;
  OpusEncoder& operator=(const OpusEncoder&) = delete;

  // Enable move
  OpusEncoder(OpusEncoder&&) noexcept;
  OpusEncoder& operator=(OpusEncoder&&) noexcept;

  /**
   * @brief Initialize the encoder
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Encode PCM samples to Opus
   * @param pcm_samples PCM samples (16-bit signed, interleaved if stereo)
   * @return Encoded result
   */
  [[nodiscard]] EncodeResult encode(std::span<const int16_t> pcm_samples);

  /**
   * @brief Set target bitrate
   * @param bitrate_bps Bitrate in bits per second
   */
  void set_bitrate(int bitrate_bps);

  /**
   * @brief Set encoding complexity
   * @param complexity 0-10 (higher = better quality, more CPU)
   */
  void set_complexity(int complexity);

  /**
   * @brief Enable/disable discontinuous transmission (DTX)
   */
  void set_dtx(bool enable);

  /**
   * @brief Reset encoder state (call after packet loss)
   */
  void reset();

  /**
   * @brief Get frame size in samples
   */
  [[nodiscard]] int frame_size() const;

  /**
   * @brief Check if encoder is initialized
   */
  [[nodiscard]] bool is_initialized() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Opus audio decoder
 *
 * Decodes Opus packets to raw PCM audio for playback.
 */
class OpusDecoder
{
 public:
  explicit OpusDecoder(OpusDecoderConfig config = {});
  ~OpusDecoder();

  // Disable copy
  OpusDecoder(const OpusDecoder&) = delete;
  OpusDecoder& operator=(const OpusDecoder&) = delete;

  // Enable move
  OpusDecoder(OpusDecoder&&) noexcept;
  OpusDecoder& operator=(OpusDecoder&&) noexcept;

  /**
   * @brief Initialize the decoder
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Decode Opus packet to PCM
   * @param opus_data Encoded Opus packet
   * @param frame_size Expected frame size in samples
   * @return Decoded result
   */
  [[nodiscard]] DecodeResult decode(std::span<const uint8_t> opus_data, int frame_size);

  /**
   * @brief Generate packet loss concealment (PLC)
   * @param frame_size Frame size in samples
   * @return Concealed samples
   */
  [[nodiscard]] DecodeResult decode_plc(int frame_size);

  /**
   * @brief Reset decoder state
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

}  // namespace audio
}  // namespace rtc
