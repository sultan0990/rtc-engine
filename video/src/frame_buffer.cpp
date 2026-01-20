/**
 * @file frame_buffer.cpp
 * @brief Frame buffer implementation
 */

#include "rtc/video/frame_buffer.h"

#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <set>

#include "rtc/video/video_codec.h"


namespace rtc
{
namespace video
{

struct FrameAssembler
{
  uint32_t timestamp = 0;
  std::map<uint16_t, std::vector<uint8_t>> packets;
  uint16_t first_sequence = 0;
  uint16_t last_sequence = 0;
  bool has_first = false;
  bool has_last = false;
  bool is_keyframe = false;
  std::chrono::steady_clock::time_point first_arrival;

  bool is_complete() const
  {
    if (!has_first || !has_last) return false;

    // Check for gaps
    for (uint16_t seq = first_sequence; seq != last_sequence + 1; ++seq)
    {
      if (packets.find(seq) == packets.end())
      {
        return false;
      }
    }
    return true;
  }

  BufferedFrame assemble() const
  {
    BufferedFrame frame;
    frame.rtp_timestamp = timestamp;
    frame.sequence_start = first_sequence;
    frame.sequence_end = last_sequence;
    frame.arrival_time = first_arrival;
    frame.is_keyframe = is_keyframe;
    frame.is_complete = true;

    // Concatenate packets in order
    for (uint16_t seq = first_sequence;; ++seq)
    {
      auto it = packets.find(seq);
      if (it != packets.end())
      {
        frame.data.insert(frame.data.end(), it->second.begin(), it->second.end());
      }
      if (seq == last_sequence) break;
    }

    return frame;
  }
};

struct FrameBuffer::Impl
{
  FrameBufferConfig config;
  mutable std::mutex mutex;

  std::map<uint32_t, FrameAssembler> assemblers;  // By timestamp
  std::deque<BufferedFrame> complete_frames;
  std::set<uint16_t> received_sequences;
  uint16_t highest_sequence = 0;
  bool has_keyframe = false;

  FrameBufferStats stats;

  Impl(FrameBufferConfig cfg) : config(std::move(cfg)) {}

  void cleanup_old_frames()
  {
    // Remove frames older than max_delay
    auto now = std::chrono::steady_clock::now();

    while (!complete_frames.empty())
    {
      auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - complete_frames.front().arrival_time);
      if (age > config.max_delay)
      {
        complete_frames.pop_front();
        stats.frames_dropped++;
      }
      else
      {
        break;
      }
    }

    // Remove incomplete assemblers that are too old
    for (auto it = assemblers.begin(); it != assemblers.end();)
    {
      auto age =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.first_arrival);
      if (age > config.max_delay * 2)
      {
        it = assemblers.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
};

FrameBuffer::FrameBuffer(FrameBufferConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

FrameBuffer::~FrameBuffer() = default;

void FrameBuffer::insert_packet(std::span<const uint8_t> data, uint16_t sequence,
                                uint32_t timestamp, bool marker, bool is_keyframe_packet)
{
  std::lock_guard lock(impl_->mutex);

  // Track sequence numbers
  impl_->received_sequences.insert(sequence);

  // Track highest sequence for NACK
  int16_t diff = static_cast<int16_t>(sequence - impl_->highest_sequence);
  if (diff > 0)
  {
    impl_->highest_sequence = sequence;
  }

  // Find or create assembler
  auto& assembler = impl_->assemblers[timestamp];
  if (assembler.packets.empty())
  {
    assembler.timestamp = timestamp;
    assembler.first_arrival = std::chrono::steady_clock::now();
  }

  // Store packet
  assembler.packets[sequence] = std::vector<uint8_t>(data.begin(), data.end());

  // Track first packet (determined by sequence)
  if (!assembler.has_first || static_cast<int16_t>(sequence - assembler.first_sequence) < 0)
  {
    assembler.first_sequence = sequence;
    assembler.has_first = true;
  }

  // Track last packet (marker bit)
  if (marker)
  {
    assembler.last_sequence = sequence;
    assembler.has_last = true;
  }

  if (is_keyframe_packet)
  {
    assembler.is_keyframe = true;
  }

  // Check if frame is complete
  if (assembler.is_complete())
  {
    // If waiting for keyframe and this isn't one, skip
    if (impl_->config.wait_for_keyframe && !impl_->has_keyframe && !assembler.is_keyframe)
    {
      return;
    }

    if (assembler.is_keyframe)
    {
      impl_->has_keyframe = true;
    }

    impl_->complete_frames.push_back(assembler.assemble());
    impl_->assemblers.erase(timestamp);
    impl_->stats.frames_buffered++;
  }

  impl_->cleanup_old_frames();
}

std::optional<BufferedFrame> FrameBuffer::pop_frame()
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->complete_frames.empty())
  {
    return std::nullopt;
  }

  // Check if target delay has passed
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - impl_->complete_frames.front().arrival_time);

  if (age < impl_->config.target_delay)
  {
    return std::nullopt;
  }

  auto frame = std::move(impl_->complete_frames.front());
  impl_->complete_frames.pop_front();
  impl_->stats.frames_decoded++;

  return frame;
}

std::optional<BufferedFrame> FrameBuffer::peek_frame() const
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->complete_frames.empty())
  {
    return std::nullopt;
  }

  return impl_->complete_frames.front();
}

bool FrameBuffer::has_complete_frame() const
{
  std::lock_guard lock(impl_->mutex);
  return !impl_->complete_frames.empty();
}

std::vector<uint16_t> FrameBuffer::get_nack_list(size_t max_count) const
{
  std::lock_guard lock(impl_->mutex);
  std::vector<uint16_t> nacks;

  // Find missing sequences in recent range
  uint16_t start = impl_->highest_sequence - 100;
  for (uint16_t seq = start; seq != impl_->highest_sequence && nacks.size() < max_count; ++seq)
  {
    if (impl_->received_sequences.find(seq) == impl_->received_sequences.end())
    {
      nacks.push_back(seq);
      impl_->stats.packets_lost++;
    }
  }

  return nacks;
}

bool FrameBuffer::should_request_keyframe() const
{
  std::lock_guard lock(impl_->mutex);

  // Request keyframe if too many frames lost or no keyframe received
  if (!impl_->has_keyframe)
  {
    return true;
  }

  return impl_->stats.frames_dropped > 10;
}

FrameBufferStats FrameBuffer::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

void FrameBuffer::reset()
{
  std::lock_guard lock(impl_->mutex);
  impl_->assemblers.clear();
  impl_->complete_frames.clear();
  impl_->received_sequences.clear();
  impl_->has_keyframe = false;
  impl_->stats = {};
}

}  // namespace video
}  // namespace rtc
