#pragma once

/**
 * @file audio_capture.h
 * @brief Cross-platform audio capture using PortAudio
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rtc
{
namespace audio
{

/**
 * @brief Audio device information
 */
struct AudioDevice
{
  int id = -1;
  std::string name;
  int max_input_channels = 0;
  int max_output_channels = 0;
  double default_sample_rate = 48000.0;
  bool is_default_input = false;
  bool is_default_output = false;
};

/**
 * @brief Audio capture configuration
 */
struct AudioCaptureConfig
{
  int device_id = -1;          // -1 for default device
  int sample_rate = 48000;     // Sample rate in Hz
  int channels = 1;            // Number of channels
  int frame_duration_ms = 20;  // Frame duration (10, 20, 40, 60)
  int buffer_frames = 4;       // Number of frames to buffer
};

/**
 * @brief Callback for captured audio frames
 * @param samples PCM samples (16-bit signed, interleaved if stereo)
 * @param timestamp Capture timestamp in microseconds
 */
using AudioCaptureCallback =
    std::function<void(std::span<const int16_t> samples, int64_t timestamp)>;

/**
 * @brief Cross-platform audio capture
 */
class AudioCapture
{
 public:
  AudioCapture();
  ~AudioCapture();

  // Disable copy
  AudioCapture(const AudioCapture&) = delete;
  AudioCapture& operator=(const AudioCapture&) = delete;

  /**
   * @brief Initialize audio system
   * @return True if successful
   */
  [[nodiscard]] static bool initialize_audio_system();

  /**
   * @brief Terminate audio system
   */
  static void terminate_audio_system();

  /**
   * @brief Get list of available audio devices
   */
  [[nodiscard]] static std::vector<AudioDevice> get_devices();

  /**
   * @brief Get default input device
   */
  [[nodiscard]] static std::optional<AudioDevice> get_default_input_device();

  /**
   * @brief Open capture device
   * @param config Capture configuration
   * @return True if successful
   */
  [[nodiscard]] bool open(AudioCaptureConfig config);

  /**
   * @brief Start capturing audio
   * @param callback Called for each captured frame
   * @return True if successful
   */
  [[nodiscard]] bool start(AudioCaptureCallback callback);

  /**
   * @brief Stop capturing
   */
  void stop();

  /**
   * @brief Close the capture device
   */
  void close();

  /**
   * @brief Check if capturing
   */
  [[nodiscard]] bool is_capturing() const;

  /**
   * @brief Get frame size in samples
   */
  [[nodiscard]] int frame_size() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Cross-platform audio playback
 */
class AudioPlayback
{
 public:
  AudioPlayback();
  ~AudioPlayback();

  // Disable copy
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

  /**
   * @brief Open playback device
   * @param device_id Device ID (-1 for default)
   * @param sample_rate Sample rate in Hz
   * @param channels Number of channels
   * @return True if successful
   */
  [[nodiscard]] bool open(int device_id, int sample_rate, int channels);

  /**
   * @brief Start playback
   */
  [[nodiscard]] bool start();

  /**
   * @brief Write samples to playback buffer
   * @param samples PCM samples (16-bit signed)
   * @return Number of samples written
   */
  size_t write(std::span<const int16_t> samples);

  /**
   * @brief Stop playback
   */
  void stop();

  /**
   * @brief Close playback device
   */
  void close();

  /**
   * @brief Check if playing
   */
  [[nodiscard]] bool is_playing() const;

  /**
   * @brief Get available buffer space in samples
   */
  [[nodiscard]] size_t available_buffer_space() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace audio
}  // namespace rtc
