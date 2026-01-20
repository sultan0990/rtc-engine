#pragma once

/**
 * @file lock_free_queue.h
 * @brief Lock-free SPSC/MPSC queue for real-time audio/video processing
 *
 * Uses atomics for thread-safe, lock-free operation.
 * Critical for low-latency media pipelines.
 */

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace rtc
{

/**
 * @brief Single-Producer Single-Consumer lock-free queue
 *
 * Optimized for audio/video pipelines where one thread produces
 * and another consumes (e.g., capture thread -> encode thread).
 *
 * @tparam T Element type
 */
template <typename T>
class SpscQueue
{
 public:
  explicit SpscQueue(size_t capacity) : buffer_(capacity + 1), capacity_(capacity + 1)
  {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  /**
   * @brief Push element (producer thread only)
   * @return True if pushed, false if queue full
   */
  bool push(const T& item)
  {
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = (current_tail + 1) % capacity_;

    if (next_tail == head_.load(std::memory_order_acquire))
    {
      return false;  // Queue full
    }

    buffer_[current_tail] = item;
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  /**
   * @brief Push element (move version)
   */
  bool push(T&& item)
  {
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = (current_tail + 1) % capacity_;

    if (next_tail == head_.load(std::memory_order_acquire))
    {
      return false;
    }

    buffer_[current_tail] = std::move(item);
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pop element (consumer thread only)
   * @return Element or nullopt if queue empty
   */
  std::optional<T> pop()
  {
    const size_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire))
    {
      return std::nullopt;  // Queue empty
    }

    T item = std::move(buffer_[current_head]);
    head_.store((current_head + 1) % capacity_, std::memory_order_release);
    return item;
  }

  /**
   * @brief Check if queue is empty
   */
  [[nodiscard]] bool empty() const
  {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

  /**
   * @brief Get approximate size
   */
  [[nodiscard]] size_t size() const
  {
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t tail = tail_.load(std::memory_order_acquire);
    return (tail >= head) ? (tail - head) : (capacity_ - head + tail);
  }

  /**
   * @brief Get capacity
   */
  [[nodiscard]] size_t capacity() const
  {
    return capacity_ - 1;
  }

 private:
  std::vector<T> buffer_;
  size_t capacity_;
  alignas(64) std::atomic<size_t> head_;  // Cache-line aligned
  alignas(64) std::atomic<size_t> tail_;
};

/**
 * @brief Multi-Producer Single-Consumer lock-free queue
 *
 * For scenarios where multiple threads push to a single consumer
 * (e.g., multiple receive threads -> single decode thread).
 *
 * @tparam T Element type
 */
template <typename T>
class MpscQueue
{
 public:
  explicit MpscQueue(size_t /*capacity*/)
  {
    stub_ = new Node();
    head_.store(stub_, std::memory_order_relaxed);
    tail_.store(stub_, std::memory_order_relaxed);
  }

  ~MpscQueue()
  {
    // Drain remaining nodes
    while (pop().has_value())
    {
    }
    delete stub_;
  }

  // Disable copy
  MpscQueue(const MpscQueue&) = delete;
  MpscQueue& operator=(const MpscQueue&) = delete;

  /**
   * @brief Push element (any thread)
   */
  void push(T item)
  {
    Node* node = new Node();
    node->data = std::move(item);
    node->next.store(nullptr, std::memory_order_relaxed);

    Node* prev = head_.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);
  }

  /**
   * @brief Pop element (consumer thread only)
   */
  std::optional<T> pop()
  {
    Node* tail = tail_.load(std::memory_order_relaxed);
    Node* next = tail->next.load(std::memory_order_acquire);

    if (next == nullptr)
    {
      return std::nullopt;  // Empty
    }

    tail_.store(next, std::memory_order_release);
    T item = std::move(next->data);
    delete tail;
    return item;
  }

  /**
   * @brief Check if queue is empty
   */
  [[nodiscard]] bool empty() const
  {
    Node* tail = tail_.load(std::memory_order_acquire);
    return tail->next.load(std::memory_order_acquire) == nullptr;
  }

 private:
  struct Node
  {
    T data;
    std::atomic<Node*> next{nullptr};
  };

  alignas(64) std::atomic<Node*> head_;
  alignas(64) std::atomic<Node*> tail_;
  Node* stub_;
};

/**
 * @brief Ring buffer for audio samples
 *
 * Optimized for fixed-size audio frame transfers.
 */
class AudioRingBuffer
{
 public:
  explicit AudioRingBuffer(size_t capacity_samples);
  ~AudioRingBuffer();

  /**
   * @brief Write samples (producer)
   * @return Number of samples written
   */
  size_t write(const int16_t* samples, size_t count);

  /**
   * @brief Read samples (consumer)
   * @return Number of samples read
   */
  size_t read(int16_t* samples, size_t count);

  /**
   * @brief Available samples to read
   */
  [[nodiscard]] size_t available() const;

  /**
   * @brief Available space to write
   */
  [[nodiscard]] size_t space() const;

  /**
   * @brief Clear buffer
   */
  void clear();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
