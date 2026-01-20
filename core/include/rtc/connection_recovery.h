#pragma once

/**
 * @file connection_recovery.h
 * @brief Automatic connection recovery and ICE restart
 *
 * Handles network failures and automatic reconnection.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace rtc
{

/**
 * @brief Connection state
 */
enum class ConnectionState
{
  NEW,
  CONNECTING,
  CONNECTED,
  DISCONNECTED,
  RECONNECTING,
  FAILED,
  CLOSED,
};

/**
 * @brief Reconnection strategy
 */
enum class ReconnectStrategy
{
  NONE,                 // No automatic reconnection
  IMMEDIATE,            // Reconnect immediately
  EXPONENTIAL_BACKOFF,  // Exponential backoff
  LINEAR_BACKOFF,       // Linear backoff
};

/**
 * @brief Recovery configuration
 */
struct RecoveryConfig
{
  ReconnectStrategy strategy = ReconnectStrategy::EXPONENTIAL_BACKOFF;
  std::chrono::milliseconds initial_delay{100};
  std::chrono::milliseconds max_delay{30000};
  float backoff_multiplier = 2.0f;
  int max_attempts = 10;
  bool enable_ice_restart = true;
  std::chrono::seconds connection_timeout{10};
};

/**
 * @brief Recovery event type
 */
enum class RecoveryEvent
{
  DISCONNECTED,
  RECONNECTING,
  RECONNECTED,
  FAILED,
  ICE_RESTART,
};

/**
 * @brief Recovery event callback
 */
using RecoveryCallback = std::function<void(RecoveryEvent event, const std::string& reason)>;

/**
 * @brief Recovery statistics
 */
struct RecoveryStats
{
  size_t disconnect_count = 0;
  size_t reconnect_success = 0;
  size_t reconnect_failed = 0;
  size_t ice_restarts = 0;
  std::chrono::milliseconds total_downtime{0};
  std::chrono::milliseconds average_recovery_time{0};
};

/**
 * @brief Connection recovery manager
 *
 * Handles:
 * - Automatic reconnection with backoff
 * - ICE restart for network changes
 * - Network quality monitoring
 * - Failover to backup servers
 */
class ConnectionRecovery
{
 public:
  explicit ConnectionRecovery(RecoveryConfig config = {});
  ~ConnectionRecovery();

  // Disable copy
  ConnectionRecovery(const ConnectionRecovery&) = delete;
  ConnectionRecovery& operator=(const ConnectionRecovery&) = delete;

  /**
   * @brief Set recovery callback
   */
  void set_callback(RecoveryCallback callback);

  /**
   * @brief Report connection state change
   */
  void on_state_change(ConnectionState new_state);

  /**
   * @brief Report network change (e.g., WiFi <-> Mobile)
   */
  void on_network_change(bool has_connectivity);

  /**
   * @brief Start recovery process
   * @return True if recovery started
   */
  bool start_recovery();

  /**
   * @brief Cancel ongoing recovery
   */
  void cancel_recovery();

  /**
   * @brief Report successful reconnection
   */
  void on_reconnected();

  /**
   * @brief Get current connection state
   */
  [[nodiscard]] ConnectionState state() const;

  /**
   * @brief Get recovery statistics
   */
  [[nodiscard]] RecoveryStats stats() const;

  /**
   * @brief Get current retry attempt number
   */
  [[nodiscard]] int current_attempt() const;

  /**
   * @brief Get next retry delay
   */
  [[nodiscard]] std::chrono::milliseconds next_delay() const;

  /**
   * @brief Check if recovery is in progress
   */
  [[nodiscard]] bool is_recovering() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
