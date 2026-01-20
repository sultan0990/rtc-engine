#pragma once

/**
 * @file sfu_server.h
 * @brief Main SFU server API
 *
 * High-level API for running a Selective Forwarding Unit.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace rtc
{
namespace server
{

/**
 * @brief Server configuration
 */
struct SfuServerConfig
{
  std::string bind_address = "0.0.0.0";
  uint16_t rtp_port_min = 10000;
  uint16_t rtp_port_max = 20000;
  size_t max_rooms = 1000;
  size_t max_participants_per_room = 100;
  size_t io_threads = 4;
  bool enable_prometheus_metrics = true;
  uint16_t metrics_port = 9090;
};

/**
 * @brief Server statistics
 */
struct SfuServerStats
{
  size_t active_rooms = 0;
  size_t total_participants = 0;
  size_t audio_streams = 0;
  size_t video_streams = 0;
  uint64_t packets_per_second = 0;
  uint64_t bytes_per_second = 0;
  float cpu_usage_percent = 0.0f;
  size_t memory_usage_mb = 0;
};

/**
 * @brief Main SFU server
 *
 * Usage:
 * @code
 * SfuServer server({.bind_address = "0.0.0.0", .rtp_port_min = 10000});
 * server.start();
 * // ... server runs
 * server.stop();
 * @endcode
 */
class SfuServer
{
 public:
  explicit SfuServer(SfuServerConfig config = {});
  ~SfuServer();

  // Disable copy
  SfuServer(const SfuServer&) = delete;
  SfuServer& operator=(const SfuServer&) = delete;

  /**
   * @brief Start the server
   * @return True if started successfully
   */
  bool start();

  /**
   * @brief Stop the server
   */
  void stop();

  /**
   * @brief Check if server is running
   */
  [[nodiscard]] bool is_running() const;

  /**
   * @brief Get server statistics
   */
  [[nodiscard]] SfuServerStats stats() const;

  /**
   * @brief Get room manager for direct access
   */
  class RoomManager& room_manager();

  /**
   * @brief Get RTP forwarder for direct access
   */
  class RtpForwarder& rtp_forwarder();

  /**
   * @brief Get subscription manager for direct access
   */
  class SubscriptionManager& subscription_manager();

  /**
   * @brief Allocate an RTP port
   * @return Allocated port or 0 on failure
   */
  [[nodiscard]] uint16_t allocate_port();

  /**
   * @brief Release an allocated RTP port
   */
  void release_port(uint16_t port);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
