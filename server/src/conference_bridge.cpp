/**
 * @file conference_bridge.cpp
 * @brief Conference bridge implementation
 */

#include "rtc/server/conference_bridge.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>

#include "rtc/server/audio_mixer.h"
#include "rtc/server/video_compositor.h"


namespace rtc
{
namespace server
{

struct ConferenceBridge::Impl
{
  BridgeConfig config;
  mutable std::mutex mutex;

  std::unique_ptr<AudioMixer> audio_mixer;
  std::unique_ptr<VideoCompositor> video_compositor;

  std::unordered_set<ParticipantId> participants;
  BridgeOutputCallback output_callback;

  std::atomic<bool> running{false};
  std::thread processing_thread;
  BridgeStats stats;

  Impl(BridgeConfig cfg) : config(std::move(cfg))
  {
    audio_mixer = std::make_unique<AudioMixer>(AudioSourceConfig{
        .sample_rate = config.audio_sample_rate,
        .channels = config.audio_channels,
        .frame_duration_ms = 20,
    });

    if (config.mode == BridgeMode::AUDIO_VIDEO)
    {
      video_compositor = std::make_unique<VideoCompositor>(CompositorConfig{
          .output_width = config.video_width,
          .output_height = config.video_height,
          .output_fps = config.video_fps,
      });
    }
  }

  void processing_loop()
  {
    auto frame_duration = std::chrono::milliseconds(1000 / config.video_fps);

    while (running.load())
    {
      auto start = std::chrono::steady_clock::now();

      {
        std::lock_guard lock(mutex);
        audio_mixer->process();

        if (video_compositor)
        {
          video_compositor->process();
        }
      }

      auto elapsed = std::chrono::steady_clock::now() - start;
      auto sleep_time = frame_duration - elapsed;
      if (sleep_time > std::chrono::milliseconds(0))
      {
        std::this_thread::sleep_for(sleep_time);
      }
    }
  }
};

ConferenceBridge::ConferenceBridge(BridgeConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

ConferenceBridge::~ConferenceBridge()
{
  stop();
}

bool ConferenceBridge::start()
{
  if (impl_->running.load()) return false;

  impl_->running.store(true);
  impl_->processing_thread = std::thread([this]() { impl_->processing_loop(); });

  return true;
}

void ConferenceBridge::stop()
{
  if (!impl_->running.load()) return;

  impl_->running.store(false);
  if (impl_->processing_thread.joinable())
  {
    impl_->processing_thread.join();
  }
}

bool ConferenceBridge::is_running() const
{
  return impl_->running.load();
}

void ConferenceBridge::set_output_callback(BridgeOutputCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->output_callback = std::move(callback);
}

bool ConferenceBridge::add_participant(const ParticipantId& participant_id,
                                       const std::string& display_name)
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->participants.size() >= static_cast<size_t>(impl_->config.max_participants))
  {
    return false;
  }

  if (impl_->participants.count(participant_id) > 0)
  {
    return false;  // Already exists
  }

  impl_->participants.insert(participant_id);
  impl_->audio_mixer->add_source(participant_id);

  if (impl_->video_compositor)
  {
    impl_->video_compositor->add_source(participant_id, display_name);
  }

  impl_->stats.participant_count = impl_->participants.size();
  return true;
}

void ConferenceBridge::remove_participant(const ParticipantId& participant_id)
{
  std::lock_guard lock(impl_->mutex);

  impl_->participants.erase(participant_id);
  impl_->audio_mixer->remove_source(participant_id);

  if (impl_->video_compositor)
  {
    impl_->video_compositor->remove_source(participant_id);
  }

  impl_->stats.participant_count = impl_->participants.size();
}

void ConferenceBridge::push_audio(const ParticipantId& participant_id,
                                  std::span<const int16_t> samples, uint32_t timestamp)
{
  std::lock_guard lock(impl_->mutex);
  impl_->audio_mixer->push_audio(participant_id, samples, timestamp);
}

void ConferenceBridge::push_video(const ParticipantId& participant_id,
                                  std::span<const uint8_t> yuv_data, int width, int height,
                                  uint32_t timestamp)
{
  if (!impl_->video_compositor) return;

  std::lock_guard lock(impl_->mutex);

  video::VideoFrame frame;
  frame.width = width;
  frame.height = height;
  frame.timestamp_us = timestamp;

  // Assume I420 format
  size_t y_size = width * height;
  size_t uv_size = y_size / 4;

  if (yuv_data.size() >= y_size + uv_size * 2)
  {
    frame.data_y.assign(yuv_data.begin(), yuv_data.begin() + y_size);
    frame.data_u.assign(yuv_data.begin() + y_size, yuv_data.begin() + y_size + uv_size);
    frame.data_v.assign(yuv_data.begin() + y_size + uv_size, yuv_data.end());
    frame.stride_y = width;
    frame.stride_u = width / 2;
    frame.stride_v = width / 2;

    impl_->video_compositor->push_frame(participant_id, frame);
  }
}

void ConferenceBridge::set_audio_params(const ParticipantId& participant_id,
                                        const MixingParams& params)
{
  std::lock_guard lock(impl_->mutex);
  impl_->audio_mixer->set_mixing_params(participant_id, params);
}

void ConferenceBridge::set_layout(LayoutType layout)
{
  if (!impl_->video_compositor) return;

  std::lock_guard lock(impl_->mutex);
  impl_->video_compositor->set_layout(layout);
}

void ConferenceBridge::set_muted(const ParticipantId& participant_id, bool muted)
{
  std::lock_guard lock(impl_->mutex);
  impl_->audio_mixer->set_mixing_params(participant_id, {.muted = muted});
}

void ConferenceBridge::set_video_hidden(const ParticipantId& participant_id, bool hidden)
{
  if (!impl_->video_compositor) return;

  std::lock_guard lock(impl_->mutex);
  impl_->video_compositor->set_tile_position(participant_id, {.visible = !hidden});
}

ParticipantId ConferenceBridge::get_active_speaker() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->audio_mixer->get_active_speaker();
}

BridgeStats ConferenceBridge::stats() const
{
  std::lock_guard lock(impl_->mutex);

  BridgeStats s = impl_->stats;
  auto mixer_stats = impl_->audio_mixer->stats();
  s.audio_streams = mixer_stats.active_sources;

  if (impl_->video_compositor)
  {
    auto comp_stats = impl_->video_compositor->stats();
    s.video_streams = comp_stats.active_sources;
  }

  return s;
}

size_t ConferenceBridge::participant_count() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->participants.size();
}

AudioMixer& ConferenceBridge::audio_mixer()
{
  return *impl_->audio_mixer;
}

VideoCompositor& ConferenceBridge::video_compositor()
{
  return *impl_->video_compositor;
}

}  // namespace server
}  // namespace rtc
