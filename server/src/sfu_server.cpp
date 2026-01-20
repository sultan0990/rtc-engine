/**
 * @file sfu_server.cpp
 * @brief Main SFU server implementation
 */

#include "rtc/server/sfu_server.h"

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "rtc/server/room_manager.h"
#include "rtc/server/rtp_forwarder.h"
#include "rtc/server/subscription_manager.h"
#include "rtc/udp_socket.h"


namespace rtc
{
namespace server
{

struct SfuServer::Impl
{
  SfuServerConfig config;

  // Core components
  std::unique_ptr<RoomManager> room_manager;
  std::unique_ptr<RtpForwarder> rtp_forwarder;
  std::unique_ptr<SubscriptionManager> subscription_manager;

  // Networking
  std::vector<std::unique_ptr<UdpSocket>> sockets;
  std::vector<std::thread> io_threads;

  // Port allocation
  std::mutex port_mutex;
  std::set<uint16_t> allocated_ports;
  uint16_t next_port = 0;

  // State
  std::atomic<bool> running{false};
  SfuServerStats stats;

  Impl(SfuServerConfig cfg)
      : config(std::move(cfg)),
        room_manager(std::make_unique<RoomManager>()),
        rtp_forwarder(std::make_unique<RtpForwarder>()),
        subscription_manager(std::make_unique<SubscriptionManager>())
  {
    next_port = config.rtp_port_min;

    // Set up forwarder callback
    rtp_forwarder->set_forward_callback(
        [](const ParticipantId& /*subscriber*/, std::span<const uint8_t> /*packet*/,
           const SocketAddress& /*dest*/)
        {
          // TODO: Actually send via socket
        });
  }

  uint16_t allocate_port()
  {
    std::lock_guard lock(port_mutex);

    for (uint16_t port = next_port; port <= config.rtp_port_max; ++port)
    {
      if (allocated_ports.count(port) == 0)
      {
        allocated_ports.insert(port);
        next_port = port + 1;
        if (next_port > config.rtp_port_max)
        {
          next_port = config.rtp_port_min;
        }
        return port;
      }
    }

    // Wrap around and try again
    for (uint16_t port = config.rtp_port_min; port < next_port; ++port)
    {
      if (allocated_ports.count(port) == 0)
      {
        allocated_ports.insert(port);
        next_port = port + 1;
        return port;
      }
    }

    return 0;  // No ports available
  }

  void release_port(uint16_t port)
  {
    std::lock_guard lock(port_mutex);
    allocated_ports.erase(port);
  }

  void io_loop(size_t thread_id)
  {
    (void)thread_id;  // Unused for now

    while (running.load())
    {
      // TODO: epoll-based IO loop
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      // Process subscriptions periodically
      subscription_manager->process();

      // Cleanup rooms periodically
      room_manager->cleanup();
    }
  }
};

SfuServer::SfuServer(SfuServerConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

SfuServer::~SfuServer()
{
  stop();
}

bool SfuServer::start()
{
  if (impl_->running.load())
  {
    return false;
  }

  impl_->running.store(true);

  // Start IO threads
  for (size_t i = 0; i < impl_->config.io_threads; ++i)
  {
    impl_->io_threads.emplace_back([this, i]() { impl_->io_loop(i); });
  }

  return true;
}

void SfuServer::stop()
{
  if (!impl_->running.load())
  {
    return;
  }

  impl_->running.store(false);

  for (auto& thread : impl_->io_threads)
  {
    if (thread.joinable())
    {
      thread.join();
    }
  }
  impl_->io_threads.clear();
}

bool SfuServer::is_running() const
{
  return impl_->running.load();
}

SfuServerStats SfuServer::stats() const
{
  SfuServerStats s;
  s.active_rooms = impl_->room_manager->room_count();
  s.total_participants = impl_->room_manager->total_participants();

  auto forwarder_stats = impl_->rtp_forwarder->stats();
  s.audio_streams = forwarder_stats.active_publishers;
  s.video_streams = 0;  // TODO: Track separately

  return s;
}

RoomManager& SfuServer::room_manager()
{
  return *impl_->room_manager;
}

RtpForwarder& SfuServer::rtp_forwarder()
{
  return *impl_->rtp_forwarder;
}

SubscriptionManager& SfuServer::subscription_manager()
{
  return *impl_->subscription_manager;
}

uint16_t SfuServer::allocate_port()
{
  return impl_->allocate_port();
}

void SfuServer::release_port(uint16_t port)
{
  impl_->release_port(port);
}

}  // namespace server
}  // namespace rtc
