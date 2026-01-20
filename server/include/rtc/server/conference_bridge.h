#pragma once

/**
 * @file conference_bridge.h
 * @brief High-level MCU conference bridge
 *
 * Combines audio mixer and video compositor into a complete MCU.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace rtc
{
namespace server
{

using ParticipantId = std::string;
using RoomId = std::string;

enum class LayoutType;
struct MixingParams;

/**
 * @brief Conference bridge mode
 */
enum class BridgeMode
{
  AUDIO_ONLY,       // Audio mixing only
  AUDIO_VIDEO,      // Audio mixing + video compositing
  SFU_WITH_MIXING,  // SFU for video + MCU for audio
};

/**
 * @brief Bridge configuration
 */
struct BridgeConfig
{
  BridgeMode mode = BridgeMode::AUDIO_ONLY;
  int audio_sample_rate = 48000;
  int audio_channels = 1;
  int video_width = 1280;
  int video_height = 720;
  int video_fps = 30;
  int max_participants = 50;  // MCU limit (CPU intensive)
};

/**
 * @brief Bridge statistics
 */
struct BridgeStats
{
  size_t participant_count = 0;
  size_t audio_streams = 0;
  size_t video_streams = 0;
  float audio_cpu_percent = 0.0f;
  float video_cpu_percent = 0.0f;
  float total_latency_ms = 0.0f;
};

/**
 * @brief Callback for mixed/composited media output
 */
using BridgeOutputCallback = std::function<void(
    const ParticipantId& participant_id, std::span<const uint8_t> encoded_audio,
    std::span<const uint8_t> encoded_video, uint32_t audio_timestamp, uint32_t video_timestamp)>;

/**
 * @brief Conference bridge (MCU)
 *
 * High-level API combining audio mixing and video compositing.
 * Suitable for small-to-medium conferences (up to ~50 participants).
 */
class ConferenceBridge
{
 public:
  explicit ConferenceBridge(BridgeConfig config = {});
  ~ConferenceBridge();

  // Disable copy
  ConferenceBridge(const ConferenceBridge&) = delete;
  ConferenceBridge& operator=(const ConferenceBridge&) = delete;

  /**
   * @brief Start the bridge
   */
  bool start();

  /**
   * @brief Stop the bridge
   */
  void stop();

  /**
   * @brief Check if running
   */
  [[nodiscard]] bool is_running() const;

  /**
   * @brief Set output callback
   */
  void set_output_callback(BridgeOutputCallback callback);

  /**
   * @brief Add participant to bridge
   */
  bool add_participant(const ParticipantId& participant_id, const std::string& display_name = "");

  /**
   * @brief Remove participant from bridge
   */
  void remove_participant(const ParticipantId& participant_id);

  /**
   * @brief Push decoded audio from participant
   */
  void push_audio(const ParticipantId& participant_id, std::span<const int16_t> samples,
                  uint32_t timestamp);

  /**
   * @brief Push decoded video from participant
   */
  void push_video(const ParticipantId& participant_id, std::span<const uint8_t> yuv_data, int width,
                  int height, uint32_t timestamp);

  /**
   * @brief Set audio mixing params
   */
  void set_audio_params(const ParticipantId& participant_id, const MixingParams& params);

  /**
   * @brief Set video layout
   */
  void set_layout(LayoutType layout);

  /**
   * @brief Mute/unmute participant
   */
  void set_muted(const ParticipantId& participant_id, bool muted);

  /**
   * @brief Hide/show participant video
   */
  void set_video_hidden(const ParticipantId& participant_id, bool hidden);

  /**
   * @brief Get current active speaker
   */
  [[nodiscard]] ParticipantId get_active_speaker() const;

  /**
   * @brief Get bridge statistics
   */
  [[nodiscard]] BridgeStats stats() const;

  /**
   * @brief Get participant count
   */
  [[nodiscard]] size_t participant_count() const;

  /**
   * @brief Access audio mixer directly
   */
  class AudioMixer& audio_mixer();

  /**
   * @brief Access video compositor directly
   */
  class VideoCompositor& video_compositor();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
