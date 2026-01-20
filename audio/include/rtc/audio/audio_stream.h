#pragma once

/**
 * @file audio_stream.h
 * @brief Public API for audio streaming
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace rtc
{
namespace audio
{

/**
 * @brief Audio stream configuration
 */
struct AudioStreamConfig
{
  int sample_rate = 48000;
  int channels = 1;
  int bitrate = 32000;
  int frame_duration_ms = 20;
  bool enable_aec = true;
  bool enable_ns = true;
  bool enable_agc = true;
};

/**
 * @brief Audio stream statistics
 */
struct AudioStreamStats
{
  uint64_t packets_sent = 0;
  uint64_t packets_received = 0;
  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;
  float packet_loss_rate = 0.0f;
  float jitter_ms = 0.0f;
  float current_bitrate_kbps = 0.0f;
  float audio_level_dbfs = -96.0f;
};

/**
 * @brief Callback for encoded audio ready to send
 */
using AudioSendCallback =
    std::function<void(std::span<const uint8_t> opus_data, uint32_t timestamp, uint16_t sequence)>;

/**
 * @brief Callback for decoded audio ready for playback
 */
using AudioPlaybackCallback = std::function<void(std::span<const int16_t> pcm_samples)>;

class AudioStream;

/**
 * @brief Create an audio stream
 * @param config Stream configuration
 * @return Audio stream instance
 */
std::unique_ptr<AudioStream> create_audio_stream(AudioStreamConfig config = {});

/**
 * @brief Audio stream for sending and receiving audio
 */
class AudioStream
{
 public:
  virtual ~AudioStream() = default;

  /**
   * @brief Start the audio stream
   * @return True if successful
   */
  virtual bool start() = 0;

  /**
   * @brief Stop the audio stream
   */
  virtual void stop() = 0;

  /**
   * @brief Set callback for encoded audio packets
   */
  virtual void set_send_callback(AudioSendCallback callback) = 0;

  /**
   * @brief Set callback for decoded audio playback
   */
  virtual void set_playback_callback(AudioPlaybackCallback callback) = 0;

  /**
   * @brief Receive an encoded audio packet
   * @param opus_data Encoded Opus packet
   * @param timestamp RTP timestamp
   * @param sequence RTP sequence number
   */
  virtual void receive_packet(std::span<const uint8_t> opus_data, uint32_t timestamp,
                              uint16_t sequence) = 0;

  /**
   * @brief Get current statistics
   */
  virtual AudioStreamStats stats() const = 0;

  /**
   * @brief Mute/unmute microphone
   */
  virtual void set_muted(bool muted) = 0;

  /**
   * @brief Check if muted
   */
  virtual bool is_muted() const = 0;

  /**
   * @brief Set microphone volume (0.0 - 1.0)
   */
  virtual void set_volume(float volume) = 0;

  /**
   * @brief Get current microphone audio level in dBFS
   */
  virtual float audio_level() const = 0;

 protected:
  AudioStream() = default;
};

}  // namespace audio
}  // namespace rtc
