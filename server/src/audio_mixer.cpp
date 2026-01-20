/**
 * @file audio_mixer.cpp
 * @brief Audio mixer implementation
 */

#include "rtc/server/audio_mixer.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace rtc
{
namespace server
{

struct AudioSource
{
  ParticipantId id;
  MixingParams params;
  std::vector<int16_t> buffer;
  uint32_t last_timestamp = 0;
  float audio_level_db = -96.0f;
  bool has_data = false;
};

struct AudioMixer::Impl
{
  AudioSourceConfig config;
  mutable std::mutex mutex;

  std::unordered_map<ParticipantId, AudioSource> sources;
  MixedAudioCallback mixed_callback;
  ActiveSpeakerCallback speaker_callback;

  ParticipantId active_speaker;
  AudioMixerStats stats;

  // Mixing buffers
  std::vector<int32_t> mix_buffer;  // 32-bit for mixing headroom
  std::vector<int16_t> output_buffer;
  int frame_size = 0;

  Impl(AudioSourceConfig cfg) : config(std::move(cfg))
  {
    frame_size = config.sample_rate * config.frame_duration_ms / 1000 * config.channels;
    mix_buffer.resize(frame_size, 0);
    output_buffer.resize(frame_size, 0);
  }

  float calculate_level(const std::vector<int16_t>& samples)
  {
    if (samples.empty()) return -96.0f;

    int64_t sum_squares = 0;
    for (int16_t s : samples)
    {
      sum_squares += static_cast<int64_t>(s) * s;
    }

    double rms = std::sqrt(static_cast<double>(sum_squares) / samples.size());
    if (rms < 1.0) return -96.0f;

    return 20.0f * std::log10(rms / 32768.0);
  }

  void apply_volume_and_pan(int32_t& left, int32_t& right, const MixingParams& params,
                            int16_t sample)
  {
    if (params.muted)
    {
      return;
    }

    float vol = params.volume;
    float pan = params.pan;

    // Pan law: constant power
    float left_gain = vol * std::sqrt((1.0f - pan) / 2.0f);
    float right_gain = vol * std::sqrt((1.0f + pan) / 2.0f);

    left += static_cast<int32_t>(sample * left_gain);
    right += static_cast<int32_t>(sample * right_gain);
  }

  void update_active_speaker()
  {
    float highest_level = -96.0f;
    ParticipantId loudest;

    for (const auto& [id, source] : sources)
    {
      if (source.params.muted) continue;

      if (source.audio_level_db > highest_level)
      {
        highest_level = source.audio_level_db;
        loudest = id;
      }
    }

    // Require minimum level for active speaker (-40 dBFS)
    if (highest_level > -40.0f && loudest != active_speaker)
    {
      active_speaker = loudest;
      if (speaker_callback)
      {
        speaker_callback(active_speaker, highest_level);
      }
    }
  }
};

AudioMixer::AudioMixer(AudioSourceConfig config) : impl_(std::make_unique<Impl>(std::move(config)))
{
}

AudioMixer::~AudioMixer() = default;

void AudioMixer::set_mixed_audio_callback(MixedAudioCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->mixed_callback = std::move(callback);
}

void AudioMixer::set_active_speaker_callback(ActiveSpeakerCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->speaker_callback = std::move(callback);
}

void AudioMixer::add_source(const ParticipantId& participant_id, MixingParams params)
{
  std::lock_guard lock(impl_->mutex);

  AudioSource source;
  source.id = participant_id;
  source.params = params;
  source.buffer.resize(impl_->frame_size, 0);

  impl_->sources[participant_id] = std::move(source);
  impl_->stats.active_sources = impl_->sources.size();
}

void AudioMixer::remove_source(const ParticipantId& participant_id)
{
  std::lock_guard lock(impl_->mutex);
  impl_->sources.erase(participant_id);
  impl_->stats.active_sources = impl_->sources.size();

  if (impl_->active_speaker == participant_id)
  {
    impl_->active_speaker.clear();
  }
}

void AudioMixer::set_mixing_params(const ParticipantId& participant_id, const MixingParams& params)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->sources.find(participant_id);
  if (it != impl_->sources.end())
  {
    it->second.params = params;
  }
}

void AudioMixer::push_audio(const ParticipantId& participant_id, std::span<const int16_t> samples,
                            uint32_t timestamp)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->sources.find(participant_id);
  if (it == impl_->sources.end()) return;

  auto& source = it->second;

  // Copy samples (truncate if needed)
  size_t copy_size = std::min(samples.size(), source.buffer.size());
  std::copy_n(samples.begin(), copy_size, source.buffer.begin());

  // Calculate audio level
  source.audio_level_db = impl_->calculate_level(source.buffer);
  source.last_timestamp = timestamp;
  source.has_data = true;
}

void AudioMixer::process()
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->sources.empty()) return;

  impl_->update_active_speaker();

  // For each participant, create a mix excluding their own audio
  for (const auto& [recipient_id, _] : impl_->sources)
  {
    // Reset mix buffer
    std::fill(impl_->mix_buffer.begin(), impl_->mix_buffer.end(), 0);

    // Add all sources except recipient
    for (const auto& [source_id, source] : impl_->sources)
    {
      if (source_id == recipient_id) continue;
      if (!source.has_data) continue;
      if (source.params.muted) continue;

      // Mix mono or stereo
      if (impl_->config.channels == 1)
      {
        for (size_t i = 0; i < source.buffer.size(); ++i)
        {
          impl_->mix_buffer[i] += static_cast<int32_t>(source.buffer[i] * source.params.volume);
        }
      }
      else
      {
        // Stereo with pan
        for (size_t i = 0; i < source.buffer.size(); i += 2)
        {
          int32_t left = 0, right = 0;
          impl_->apply_volume_and_pan(left, right, source.params, source.buffer[i]);
          impl_->mix_buffer[i] += left;
          if (i + 1 < impl_->mix_buffer.size())
          {
            impl_->apply_volume_and_pan(left, right, source.params, source.buffer[i + 1]);
            impl_->mix_buffer[i + 1] += right;
          }
        }
      }
    }

    // Convert to 16-bit with saturation
    for (size_t i = 0; i < impl_->mix_buffer.size(); ++i)
    {
      int32_t val = impl_->mix_buffer[i];
      val = std::clamp(val, static_cast<int32_t>(-32768), static_cast<int32_t>(32767));
      impl_->output_buffer[i] = static_cast<int16_t>(val);
    }

    // Send to recipient
    if (impl_->mixed_callback)
    {
      uint32_t timestamp = 0;
      auto it = impl_->sources.find(recipient_id);
      if (it != impl_->sources.end())
      {
        timestamp = it->second.last_timestamp;
      }

      impl_->mixed_callback(recipient_id, impl_->output_buffer, timestamp);
    }
  }

  impl_->stats.mixed_frames++;

  // Clear has_data flags
  for (auto& [_, source] : impl_->sources)
  {
    source.has_data = false;
  }
}

ParticipantId AudioMixer::get_active_speaker() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->active_speaker;
}

size_t AudioMixer::source_count() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->sources.size();
}

AudioMixerStats AudioMixer::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

}  // namespace server
}  // namespace rtc
