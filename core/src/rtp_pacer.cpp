/**
 * @file rtp_pacer.cpp
 * @brief RTP packet pacer implementation (stub)
 */

#include "rtc/rtp_pacer.h"

#include <algorithm>
#include <queue>

#include "rtc/udp_socket.h"


namespace rtc
{

struct RtpPacer::Impl
{
  Config config;
  PacerSendCallback send_callback;

  // Token bucket state
  size_t available_tokens = 0;
  std::chrono::steady_clock::time_point last_process_time;

  // Packet queue (priority queue: higher priority first)
  struct PacketCompare
  {
    bool operator()(const PacedPacket& a, const PacedPacket& b) const
    {
      return a.priority < b.priority;  // Lower priority = later
    }
  };
  std::priority_queue<PacedPacket, std::vector<PacedPacket>, PacketCompare> queue;
  std::mutex queue_mutex;

  // Stats
  Stats stats;

  Impl(Config cfg) : config(std::move(cfg))
  {
    available_tokens = config.bucket_size_bytes;
    last_process_time = std::chrono::steady_clock::now();
  }
};

RtpPacer::RtpPacer(Config config) : impl_(std::make_unique<Impl>(std::move(config))) {}

RtpPacer::~RtpPacer() = default;

void RtpPacer::set_send_callback(PacerSendCallback callback)
{
  impl_->send_callback = std::move(callback);
}

bool RtpPacer::enqueue(std::vector<uint8_t> data, const SocketAddress& destination, int priority)
{
  std::lock_guard lock(impl_->queue_mutex);

  if (impl_->queue.size() >= impl_->config.max_queue_size)
  {
    ++impl_->stats.packets_dropped;
    return false;
  }

  PacedPacket packet;
  packet.data = std::move(data);
  packet.destination = destination;
  packet.enqueue_time = std::chrono::steady_clock::now();
  packet.priority = priority;

  impl_->queue.push(std::move(packet));
  return true;
}

size_t RtpPacer::process()
{
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - impl_->last_process_time);
  impl_->last_process_time = now;

  // Refill tokens based on elapsed time and target bitrate
  size_t new_tokens = (impl_->config.target_bitrate_bps / 8) * elapsed.count() / 1000;
  impl_->available_tokens =
      std::min(impl_->available_tokens + new_tokens, impl_->config.bucket_size_bytes);

  size_t packets_sent = 0;

  std::lock_guard lock(impl_->queue_mutex);
  while (!impl_->queue.empty())
  {
    const auto& packet = impl_->queue.top();

    // Check if we have enough tokens
    if (packet.data.size() > impl_->available_tokens)
    {
      break;  // Wait for more tokens
    }

    // Send the packet
    if (impl_->send_callback)
    {
      impl_->send_callback(packet.data, packet.destination);
    }

    impl_->available_tokens -= packet.data.size();
    impl_->stats.packets_sent++;
    impl_->stats.bytes_sent += packet.data.size();
    packets_sent++;

    impl_->queue.pop();
  }

  return packets_sent;
}

void RtpPacer::set_target_bitrate(uint64_t bitrate_bps)
{
  impl_->config.target_bitrate_bps = bitrate_bps;
}

uint64_t RtpPacer::target_bitrate() const
{
  return impl_->config.target_bitrate_bps;
}

size_t RtpPacer::queue_size() const
{
  std::lock_guard lock(impl_->queue_mutex);
  return impl_->queue.size();
}

std::chrono::milliseconds RtpPacer::queue_delay() const
{
  std::lock_guard lock(impl_->queue_mutex);
  if (impl_->queue.empty())
  {
    return std::chrono::milliseconds{0};
  }
  auto now = std::chrono::steady_clock::now();
  // Can't access top() easily for delay calculation with priority_queue
  // This is a limitation of the current implementation
  return std::chrono::milliseconds{0};
}

void RtpPacer::clear()
{
  std::lock_guard lock(impl_->queue_mutex);
  while (!impl_->queue.empty())
  {
    impl_->queue.pop();
  }
}

RtpPacer::Stats RtpPacer::stats() const
{
  return impl_->stats;
}

}  // namespace rtc
