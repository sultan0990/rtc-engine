/**
 * @file jitter_buffer.cpp
 * @brief Adaptive jitter buffer implementation
 */

#include "rtc/audio/jitter_buffer.h"

#include <algorithm>
#include <deque>
#include <mutex>

namespace rtc
{
namespace audio
{

struct JitterBuffer::Impl
{
  JitterBufferConfig config;
  std::deque<JitterFrame> buffer;
  mutable std::mutex mutex;

  // State
  uint16_t expected_sequence = 0;
  bool sequence_initialized = false;
  std::chrono::steady_clock::time_point playout_start;
  bool playout_started = false;

  // Stats
  JitterBufferStats stats;
  float jitter_estimate = 0.0f;
  int64_t last_arrival_delta = 0;

  Impl(JitterBufferConfig cfg) : config(std::move(cfg))
  {
    stats.target_delay = config.target_delay;
  }

  void update_jitter(std::chrono::steady_clock::time_point arrival_time, uint32_t /*timestamp*/)
  {
    // Simplified jitter estimation
    auto now = arrival_time;
    if (playout_started)
    {
      auto delta =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - playout_start).count();
      auto diff = std::abs(delta - last_arrival_delta);
      jitter_estimate += (diff - jitter_estimate) / 16.0f;
      last_arrival_delta = delta;
    }
    else
    {
      playout_start = now;
      playout_started = true;
    }
    stats.jitter_ms = jitter_estimate;
  }

  void adapt_delay()
  {
    if (!config.enable_adaptive)
    {
      return;
    }

    // Adjust target delay based on jitter
    auto new_delay = std::chrono::milliseconds(static_cast<int>(jitter_estimate * 2 + 10));

    new_delay = std::clamp(new_delay, config.min_delay, config.max_delay);
    stats.target_delay = new_delay;
  }
};

JitterBuffer::JitterBuffer(JitterBufferConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

JitterBuffer::~JitterBuffer() = default;

bool JitterBuffer::push(JitterFrame frame)
{
  std::lock_guard lock(impl_->mutex);

  // Check max size
  if (impl_->buffer.size() >= impl_->config.max_packets)
  {
    // Drop oldest packet
    impl_->buffer.pop_front();
    impl_->stats.packets_late++;
  }

  // Initialize sequence tracking
  if (!impl_->sequence_initialized)
  {
    impl_->expected_sequence = frame.sequence_number;
    impl_->sequence_initialized = true;
  }

  // Check for duplicates
  for (const auto& f : impl_->buffer)
  {
    if (f.sequence_number == frame.sequence_number)
    {
      impl_->stats.packets_duplicated++;
      return false;
    }
  }

  // Update jitter estimate
  impl_->update_jitter(frame.arrival_time, frame.timestamp);
  impl_->adapt_delay();

  // Insert in order
  auto it = std::find_if(impl_->buffer.begin(), impl_->buffer.end(),
                         [&](const JitterFrame& f)
                         {
                           // Handle sequence number wrap-around
                           int16_t diff =
                               static_cast<int16_t>(frame.sequence_number - f.sequence_number);
                           return diff < 0;
                         });

  impl_->buffer.insert(it, std::move(frame));
  impl_->stats.packets_received++;
  impl_->stats.current_size = impl_->buffer.size();

  return true;
}

std::optional<JitterFrame> JitterBuffer::pop()
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->buffer.empty())
  {
    return std::nullopt;
  }

  // Check if we should wait more (buffering delay)
  if (!impl_->playout_started)
  {
    return std::nullopt;
  }

  auto now = std::chrono::steady_clock::now();
  auto front_age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - impl_->buffer.front().arrival_time);

  if (front_age < impl_->stats.target_delay)
  {
    return std::nullopt;
  }

  // Get next frame
  auto frame = std::move(impl_->buffer.front());
  impl_->buffer.pop_front();

  // Check for packet loss
  if (frame.sequence_number != impl_->expected_sequence)
  {
    int16_t diff = static_cast<int16_t>(frame.sequence_number - impl_->expected_sequence);
    if (diff > 0)
    {
      impl_->stats.packets_lost += diff;
    }
  }

  impl_->expected_sequence = frame.sequence_number + 1;
  impl_->stats.current_size = impl_->buffer.size();

  // Update packet loss rate
  if (impl_->stats.packets_received > 0)
  {
    impl_->stats.packet_loss_rate =
        static_cast<float>(impl_->stats.packets_lost) /
        static_cast<float>(impl_->stats.packets_received + impl_->stats.packets_lost);
  }

  return frame;
}

std::optional<JitterFrame> JitterBuffer::peek() const
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->buffer.empty())
  {
    return std::nullopt;
  }

  return impl_->buffer.front();
}

bool JitterBuffer::is_ready() const
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->buffer.empty())
  {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  auto front_age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - impl_->buffer.front().arrival_time);

  return front_age >= impl_->stats.target_delay;
}

size_t JitterBuffer::size() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->buffer.size();
}

JitterBufferStats JitterBuffer::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

void JitterBuffer::reset()
{
  std::lock_guard lock(impl_->mutex);
  impl_->buffer.clear();
  impl_->sequence_initialized = false;
  impl_->playout_started = false;
  impl_->stats = {};
  impl_->stats.target_delay = impl_->config.target_delay;
}

void JitterBuffer::set_target_delay(std::chrono::milliseconds delay)
{
  std::lock_guard lock(impl_->mutex);
  impl_->stats.target_delay = std::clamp(delay, impl_->config.min_delay, impl_->config.max_delay);
}

}  // namespace audio
}  // namespace rtc
