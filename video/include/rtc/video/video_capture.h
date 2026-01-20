#pragma once

/**
 * @file video_capture.h
 * @brief Video capture using V4L2 (Linux)
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtc
{
namespace video
{

struct VideoFrame;

/**
 * @brief Video device information
 */
struct VideoDevice
{
  std::string path;  // e.g., "/dev/video0"
  std::string name;  // Device name
  std::vector<std::pair<int, int>> supported_resolutions;
  std::vector<int> supported_fps;
  bool is_capture_device = false;
};

/**
 * @brief Video capture configuration
 */
struct VideoCaptureConfig
{
  std::string device_path = "/dev/video0";
  int width = 1280;
  int height = 720;
  int fps = 30;
  bool prefer_mjpeg = false;  // Use MJPEG if available (lower CPU)
};

/**
 * @brief Callback for captured video frames
 */
using VideoCaptureCallback = std::function<void(const VideoFrame& frame)>;

/**
 * @brief Video capture using V4L2 (Linux)
 */
class VideoCapture
{
 public:
  VideoCapture();
  ~VideoCapture();

  // Disable copy
  VideoCapture(const VideoCapture&) = delete;
  VideoCapture& operator=(const VideoCapture&) = delete;

  /**
   * @brief Get list of available video devices
   */
  [[nodiscard]] static std::vector<VideoDevice> get_devices();

  /**
   * @brief Get default capture device
   */
  [[nodiscard]] static std::optional<VideoDevice> get_default_device();

  /**
   * @brief Open capture device
   * @param config Capture configuration
   * @return True if successful
   */
  [[nodiscard]] bool open(VideoCaptureConfig config);

  /**
   * @brief Start capturing video
   * @param callback Called for each captured frame
   * @return True if successful
   */
  [[nodiscard]] bool start(VideoCaptureCallback callback);

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
   * @brief Get current width
   */
  [[nodiscard]] int width() const;

  /**
   * @brief Get current height
   */
  [[nodiscard]] int height() const;

  /**
   * @brief Get current FPS
   */
  [[nodiscard]] int fps() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace video
}  // namespace rtc
