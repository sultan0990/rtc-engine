#pragma once

/**
 * @file video_compositor.h
 * @brief Server-side video compositing for MCU mode
 *
 * Composites multiple video streams into a single grid layout.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rtc
{
namespace video
{
struct VideoFrame;
}

namespace server
{

using ParticipantId = std::string;

/**
 * @brief Video layout type
 */
enum class LayoutType
{
  GRID,          // Equal-sized tiles in a grid
  SPOTLIGHT,     // One large + small thumbnails
  PRESENTATION,  // Presentation + small camera views
  SIDE_BY_SIDE,  // Two participants side by side
  CUSTOM,        // Custom positions
};

/**
 * @brief Video tile position
 */
struct TilePosition
{
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int z_order = 0;  // Higher = on top
  bool visible = true;
};

/**
 * @brief Compositing configuration
 */
struct CompositorConfig
{
  int output_width = 1280;
  int output_height = 720;
  int output_fps = 30;
  LayoutType layout = LayoutType::GRID;
  uint32_t background_color = 0x1A1A1A;  // Dark gray
  int border_width = 2;
  uint32_t border_color = 0x333333;
  bool show_names = true;
};

/**
 * @brief Composited video callback
 */
using CompositedVideoCallback =
    std::function<void(const video::VideoFrame& frame, uint32_t timestamp)>;

/**
 * @brief Video compositor statistics
 */
struct CompositorStats
{
  size_t active_sources = 0;
  size_t composited_frames = 0;
  float cpu_usage_percent = 0.0f;
  float average_encode_ms = 0.0f;
};

/**
 * @brief Server-side video compositor
 *
 * Features:
 * - Multiple layout modes (grid, spotlight, etc.)
 * - Dynamic tile positioning
 * - Active speaker highlighting
 * - Name overlays
 * - Background customization
 */
class VideoCompositor
{
 public:
  explicit VideoCompositor(CompositorConfig config = {});
  ~VideoCompositor();

  // Disable copy
  VideoCompositor(const VideoCompositor&) = delete;
  VideoCompositor& operator=(const VideoCompositor&) = delete;

  /**
   * @brief Set callback for composited output
   */
  void set_output_callback(CompositedVideoCallback callback);

  /**
   * @brief Add a video source
   */
  void add_source(const ParticipantId& participant_id, const std::string& display_name = "");

  /**
   * @brief Remove a video source
   */
  void remove_source(const ParticipantId& participant_id);

  /**
   * @brief Set layout type
   */
  void set_layout(LayoutType layout);

  /**
   * @brief Set custom tile position for a participant
   */
  void set_tile_position(const ParticipantId& participant_id, const TilePosition& position);

  /**
   * @brief Set active speaker (for spotlight layout)
   */
  void set_active_speaker(const ParticipantId& participant_id);

  /**
   * @brief Push video frame from a source
   */
  void push_frame(const ParticipantId& participant_id, const video::VideoFrame& frame);

  /**
   * @brief Process compositing (call every output frame period)
   */
  void process();

  /**
   * @brief Get compositor statistics
   */
  [[nodiscard]] CompositorStats stats() const;

  /**
   * @brief Get current layout type
   */
  [[nodiscard]] LayoutType layout() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
