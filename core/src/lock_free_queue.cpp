/**
 * @file lock_free_queue.cpp
 * @brief Lock-free queue implementations
 */

#include "rtc/lock_free_queue.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace rtc
{

struct AudioRingBuffer::Impl
{
  std::vector<int16_t> buffer;
  size_t capacity = 0;
  alignas(64) std::atomic<size_t> read_pos{0};
  alignas(64) std::atomic<size_t> write_pos{0};

  Impl(size_t cap) : buffer(cap), capacity(cap) {}
};

AudioRingBuffer::AudioRingBuffer(size_t capacity_samples)
    : impl_(std::make_unique<Impl>(capacity_samples))
{
}

AudioRingBuffer::~AudioRingBuffer() = default;

size_t AudioRingBuffer::write(const int16_t* samples, size_t count)
{
  size_t write_pos = impl_->write_pos.load(std::memory_order_relaxed);
  size_t read_pos = impl_->read_pos.load(std::memory_order_acquire);

  size_t available_space = (read_pos > write_pos) ? (read_pos - write_pos - 1)
                                                  : (impl_->capacity - write_pos + read_pos - 1);

  size_t to_write = std::min(count, available_space);
  if (to_write == 0) return 0;

  // Write potentially in two parts (wrap-around)
  size_t first_part = std::min(to_write, impl_->capacity - write_pos);
  std::memcpy(&impl_->buffer[write_pos], samples, first_part * sizeof(int16_t));

  if (to_write > first_part)
  {
    std::memcpy(&impl_->buffer[0], samples + first_part, (to_write - first_part) * sizeof(int16_t));
  }

  impl_->write_pos.store((write_pos + to_write) % impl_->capacity, std::memory_order_release);
  return to_write;
}

size_t AudioRingBuffer::read(int16_t* samples, size_t count)
{
  size_t read_pos = impl_->read_pos.load(std::memory_order_relaxed);
  size_t write_pos = impl_->write_pos.load(std::memory_order_acquire);

  size_t available =
      (write_pos >= read_pos) ? (write_pos - read_pos) : (impl_->capacity - read_pos + write_pos);

  size_t to_read = std::min(count, available);
  if (to_read == 0) return 0;

  // Read potentially in two parts (wrap-around)
  size_t first_part = std::min(to_read, impl_->capacity - read_pos);
  std::memcpy(samples, &impl_->buffer[read_pos], first_part * sizeof(int16_t));

  if (to_read > first_part)
  {
    std::memcpy(samples + first_part, &impl_->buffer[0], (to_read - first_part) * sizeof(int16_t));
  }

  impl_->read_pos.store((read_pos + to_read) % impl_->capacity, std::memory_order_release);
  return to_read;
}

size_t AudioRingBuffer::available() const
{
  size_t read_pos = impl_->read_pos.load(std::memory_order_acquire);
  size_t write_pos = impl_->write_pos.load(std::memory_order_acquire);

  return (write_pos >= read_pos) ? (write_pos - read_pos)
                                 : (impl_->capacity - read_pos + write_pos);
}

size_t AudioRingBuffer::space() const
{
  size_t read_pos = impl_->read_pos.load(std::memory_order_acquire);
  size_t write_pos = impl_->write_pos.load(std::memory_order_acquire);

  return (read_pos > write_pos) ? (read_pos - write_pos - 1)
                                : (impl_->capacity - write_pos + read_pos - 1);
}

void AudioRingBuffer::clear()
{
  impl_->read_pos.store(0, std::memory_order_relaxed);
  impl_->write_pos.store(0, std::memory_order_relaxed);
}

}  // namespace rtc
