#pragma once

/**
 * @file audio_mixer.h
 * @brief Server-side audio mixing for MCU mode
 *
 * Mixes multiple audio streams into a single output stream.
 * Each participant receives a unique mix excluding their own audio.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rtc
{
namespace server
{

using ParticipantId = std::string;

/**
 * @brief Audio source configuration
 */
struct AudioSourceConfig
{
  int sample_rate = 48000;
  int channels = 1;
  int frame_duration_ms = 20;
};

/**
 * @brief Mixing parameters for a participant
 */
struct MixingParams
{
  float volume = 1.0f;  // 0.0 - 2.0
  float pan = 0.0f;     // -1.0 (left) to 1.0 (right), 0.0 = center
  bool muted = false;
  bool is_active_speaker = false;
};

/**
 * @brief Mixed audio output callback
 * @param participant_id Recipient of the mixed audio
 * @param mixed_samples Mixed PCM samples (excludes recipient's own audio)
 * @param timestamp Timestamp for RTP
 */
using MixedAudioCallback =
    std::function<void(const ParticipantId& participant_id, std::span<const int16_t> mixed_samples,
                       uint32_t timestamp)>;

/**
 * @brief Active speaker callback
 * @param participant_id Current active speaker
 * @param audio_level Audio level in dBFS
 */
using ActiveSpeakerCallback =
    std::function<void(const ParticipantId& participant_id, float audio_level)>;

/**
 * @brief Audio mixer statistics
 */
struct AudioMixerStats
{
  size_t active_sources = 0;
  size_t mixed_frames = 0;
  float cpu_usage_percent = 0.0f;
  float average_latency_ms = 0.0f;
};

/**
 * @brief Server-side audio mixer
 *
 * Features:
 * - Mix N audio sources into N unique outputs (each excluding self)
 * - Per-source volume and panning control
 * - Active speaker detection
 * - Automatic gain control for mixed output
 * - Low-latency lock-free processing
 */
class AudioMixer
{
 public:
  explicit AudioMixer(AudioSourceConfig config = {});
  ~AudioMixer();

  // Disable copy
  AudioMixer(const AudioMixer&) = delete;
  AudioMixer& operator=(const AudioMixer&) = delete;

  /**
   * @brief Set callback for mixed audio output
   */
  void set_mixed_audio_callback(MixedAudioCallback callback);

  /**
   * @brief Set callback for active speaker changes
   */
  void set_active_speaker_callback(ActiveSpeakerCallback callback);

  /**
   * @brief Add an audio source (participant)
   * @param participant_id Unique identifier
   * @param params Initial mixing parameters
   */
  void add_source(const ParticipantId& participant_id, MixingParams params = {});

  /**
   * @brief Remove an audio source
   */
  void remove_source(const ParticipantId& participant_id);

  /**
   * @brief Update mixing parameters for a source
   */
  void set_mixing_params(const ParticipantId& participant_id, const MixingParams& params);

  /**
   * @brief Push audio samples from a source
   * @param participant_id Source participant
   * @param samples PCM samples (16-bit signed)
   * @param timestamp RTP timestamp
   */
  void push_audio(const ParticipantId& participant_id, std::span<const int16_t> samples,
                  uint32_t timestamp);

  /**
   * @brief Process mixing (call every frame period, e.g., 20ms)
   * This generates mixed output for all participants
   */
  void process();

  /**
   * @brief Get current active speaker
   */
  [[nodiscard]] ParticipantId get_active_speaker() const;

  /**
   * @brief Get number of active sources
   */
  [[nodiscard]] size_t source_count() const;

  /**
   * @brief Get mixer statistics
   */
  [[nodiscard]] AudioMixerStats stats() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
