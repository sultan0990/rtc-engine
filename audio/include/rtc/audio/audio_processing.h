#pragma once

/**
 * @file audio_processing.h
 * @brief Audio processing pipeline: AEC, NS, AGC
 */

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace rtc
{
namespace audio
{

/**
 * @brief Echo cancellation configuration
 */
struct AecConfig
{
  int sample_rate = 48000;
  int channels = 1;
  int frame_duration_ms = 20;
  int filter_length_ms = 128;  // Echo tail length
  bool enable_delay_agnostic = true;
  bool enable_extended_filter = true;
};

/**
 * @brief Noise suppression configuration
 */
struct NsConfig
{
  int sample_rate = 48000;
  int channels = 1;

  enum class Level
  {
    LOW,
    MODERATE,
    HIGH,
    VERY_HIGH
  };
  Level level = Level::MODERATE;
};

/**
 * @brief Automatic gain control configuration
 */
struct AgcConfig
{
  int sample_rate = 48000;
  int channels = 1;
  int target_level_dbfs = -3;   // Target output level in dBFS
  int compression_gain_db = 9;  // Maximum gain in dB
  bool limiter_enabled = true;  // Enable hard limiter

  enum class Mode
  {
    ADAPTIVE_ANALOG,
    ADAPTIVE_DIGITAL,
    FIXED_DIGITAL
  };
  Mode mode = Mode::ADAPTIVE_DIGITAL;
};

/**
 * @brief Acoustic Echo Cancellation (AEC)
 *
 * Removes echo caused by speaker-to-microphone coupling.
 * Uses WebRTC AEC3-style algorithm.
 */
class EchoCanceller
{
 public:
  explicit EchoCanceller(AecConfig config = {});
  ~EchoCanceller();

  // Disable copy
  EchoCanceller(const EchoCanceller&) = delete;
  EchoCanceller& operator=(const EchoCanceller&) = delete;

  /**
   * @brief Initialize the echo canceller
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Process a frame from the far-end (speaker output)
   * @param playback_samples Samples being played back
   */
  void analyze_render(std::span<const int16_t> playback_samples);

  /**
   * @brief Process a captured frame (near-end)
   * @param capture_samples Captured microphone samples (modified in-place)
   */
  void process_capture(std::span<int16_t> capture_samples);

  /**
   * @brief Get estimated echo return loss enhancement (ERLE) in dB
   */
  [[nodiscard]] float get_erle() const;

  /**
   * @brief Reset state
   */
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Noise Suppression (NS)
 *
 * Reduces background noise in captured audio.
 * Based on RNNoise or WebRTC NS.
 */
class NoiseSuppressor
{
 public:
  explicit NoiseSuppressor(NsConfig config = {});
  ~NoiseSuppressor();

  // Disable copy
  NoiseSuppressor(const NoiseSuppressor&) = delete;
  NoiseSuppressor& operator=(const NoiseSuppressor&) = delete;

  /**
   * @brief Initialize the noise suppressor
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Process a frame
   * @param samples Audio samples (modified in-place)
   */
  void process(std::span<int16_t> samples);

  /**
   * @brief Set suppression level
   */
  void set_level(NsConfig::Level level);

  /**
   * @brief Get voice activity probability (0.0 - 1.0)
   */
  [[nodiscard]] float get_voice_probability() const;

  /**
   * @brief Reset state
   */
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Automatic Gain Control (AGC)
 *
 * Normalizes audio levels for consistent volume.
 * Based on WebRTC AGC.
 */
class GainController
{
 public:
  explicit GainController(AgcConfig config = {});
  ~GainController();

  // Disable copy
  GainController(const GainController&) = delete;
  GainController& operator=(const GainController&) = delete;

  /**
   * @brief Initialize the gain controller
   * @return True if successful
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Process a frame
   * @param samples Audio samples (modified in-place)
   */
  void process(std::span<int16_t> samples);

  /**
   * @brief Set target output level in dBFS
   */
  void set_target_level(int level_dbfs);

  /**
   * @brief Get current gain in dB
   */
  [[nodiscard]] float get_current_gain() const;

  /**
   * @brief Check if speech is detected
   */
  [[nodiscard]] bool is_speech_detected() const;

  /**
   * @brief Reset state
   */
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Complete audio processing pipeline
 *
 * Combines AEC, NS, and AGC in the correct order.
 */
class AudioProcessor
{
 public:
  struct Config
  {
    bool enable_aec = true;
    bool enable_ns = true;
    bool enable_agc = true;
    AecConfig aec_config;
    NsConfig ns_config;
    AgcConfig agc_config;
  };

  explicit AudioProcessor(Config config = {});
  ~AudioProcessor();

  /**
   * @brief Initialize all processing components
   */
  [[nodiscard]] bool initialize();

  /**
   * @brief Process far-end (speaker) audio for echo cancellation
   */
  void process_render_frame(std::span<const int16_t> playback_samples);

  /**
   * @brief Process captured audio through the full pipeline
   * @param samples Audio samples (modified in-place)
   */
  void process_capture_frame(std::span<int16_t> samples);

  /**
   * @brief Enable/disable individual components
   */
  void set_aec_enabled(bool enabled);
  void set_ns_enabled(bool enabled);
  void set_agc_enabled(bool enabled);

  /**
   * @brief Reset all components
   */
  void reset();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace audio
}  // namespace rtc
