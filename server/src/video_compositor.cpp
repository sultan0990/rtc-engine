/**
 * @file video_compositor.cpp
 * @brief Video compositor implementation (stub)
 *
 * Note: Full implementation requires FFmpeg or similar for scaling/compositing.
 */

#include "rtc/server/video_compositor.h"

#include <mutex>
#include <unordered_map>

#include "rtc/video/video_codec.h"


namespace rtc
{
namespace server
{

struct VideoSource
{
  ParticipantId id;
  std::string display_name;
  TilePosition position;
  std::vector<uint8_t> last_frame;
  int width = 0;
  int height = 0;
  bool has_frame = false;
};

struct VideoCompositor::Impl
{
  CompositorConfig config;
  mutable std::mutex mutex;

  std::unordered_map<ParticipantId, VideoSource> sources;
  CompositedVideoCallback output_callback;
  ParticipantId active_speaker;
  LayoutType current_layout;
  CompositorStats stats;

  // Output frame buffer
  std::vector<uint8_t> output_y;
  std::vector<uint8_t> output_u;
  std::vector<uint8_t> output_v;

  Impl(CompositorConfig cfg) : config(std::move(cfg)), current_layout(cfg.layout)
  {
    // Allocate output buffers (YUV420)
    size_t y_size = config.output_width * config.output_height;
    size_t uv_size = y_size / 4;
    output_y.resize(y_size, 16);    // Black Y
    output_u.resize(uv_size, 128);  // Neutral U
    output_v.resize(uv_size, 128);  // Neutral V
  }

  void calculate_grid_positions()
  {
    if (sources.empty()) return;

    size_t count = sources.size();
    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    int rows = (count + cols - 1) / cols;

    int tile_w = config.output_width / cols;
    int tile_h = config.output_height / rows;

    int idx = 0;
    for (auto& [_, source] : sources)
    {
      int row = idx / cols;
      int col = idx % cols;

      source.position.x = col * tile_w;
      source.position.y = row * tile_h;
      source.position.width = tile_w - config.border_width * 2;
      source.position.height = tile_h - config.border_width * 2;
      source.position.visible = true;

      idx++;
    }
  }

  void fill_background()
  {
    // Fill with background color (YUV conversion)
    uint8_t bg_y = 16 + ((config.background_color >> 16) & 0xFF) * 0.299f +
                   ((config.background_color >> 8) & 0xFF) * 0.587f +
                   (config.background_color & 0xFF) * 0.114f;

    std::fill(output_y.begin(), output_y.end(), bg_y);
    std::fill(output_u.begin(), output_u.end(), 128);
    std::fill(output_v.begin(), output_v.end(), 128);
  }
};

VideoCompositor::VideoCompositor(CompositorConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

VideoCompositor::~VideoCompositor() = default;

void VideoCompositor::set_output_callback(CompositedVideoCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->output_callback = std::move(callback);
}

void VideoCompositor::add_source(const ParticipantId& participant_id,
                                 const std::string& display_name)
{
  std::lock_guard lock(impl_->mutex);

  VideoSource source;
  source.id = participant_id;
  source.display_name = display_name;

  impl_->sources[participant_id] = std::move(source);
  impl_->calculate_grid_positions();
  impl_->stats.active_sources = impl_->sources.size();
}

void VideoCompositor::remove_source(const ParticipantId& participant_id)
{
  std::lock_guard lock(impl_->mutex);
  impl_->sources.erase(participant_id);
  impl_->calculate_grid_positions();
  impl_->stats.active_sources = impl_->sources.size();
}

void VideoCompositor::set_layout(LayoutType layout)
{
  std::lock_guard lock(impl_->mutex);
  impl_->current_layout = layout;
  impl_->calculate_grid_positions();
}

void VideoCompositor::set_tile_position(const ParticipantId& participant_id,
                                        const TilePosition& position)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->sources.find(participant_id);
  if (it != impl_->sources.end())
  {
    it->second.position = position;
  }
}

void VideoCompositor::set_active_speaker(const ParticipantId& participant_id)
{
  std::lock_guard lock(impl_->mutex);
  impl_->active_speaker = participant_id;
}

void VideoCompositor::push_frame(const ParticipantId& participant_id,
                                 const video::VideoFrame& frame)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->sources.find(participant_id);
  if (it == impl_->sources.end()) return;

  auto& source = it->second;
  source.width = frame.width;
  source.height = frame.height;

  // Store frame data (simplified - just Y plane for now)
  source.last_frame = frame.data_y;
  source.has_frame = true;
}

void VideoCompositor::process()
{
  std::lock_guard lock(impl_->mutex);

  impl_->fill_background();

  // TODO: Actually composite frames (requires FFmpeg for scaling)
  // For now, just mark frames as processed

  for (auto& [_, source] : impl_->sources)
  {
    source.has_frame = false;
  }

  // Create output frame
  if (impl_->output_callback)
  {
    video::VideoFrame output;
    output.width = impl_->config.output_width;
    output.height = impl_->config.output_height;
    output.data_y = impl_->output_y;
    output.data_u = impl_->output_u;
    output.data_v = impl_->output_v;
    output.stride_y = output.width;
    output.stride_u = output.width / 2;
    output.stride_v = output.width / 2;

    impl_->output_callback(output, 0);
  }

  impl_->stats.composited_frames++;
}

CompositorStats VideoCompositor::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

LayoutType VideoCompositor::layout() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_layout;
}

}  // namespace server
}  // namespace rtc
