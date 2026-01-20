/**
 * @file connection_recovery.cpp
 * @brief Connection recovery implementation
 */

#include "rtc/connection_recovery.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace rtc
{

struct ConnectionRecovery::Impl
{
  RecoveryConfig config;
  mutable std::mutex mutex;

  RecoveryCallback callback;
  ConnectionState state = ConnectionState::NEW;
  RecoveryStats stats;

  std::atomic<bool> recovering{false};
  std::thread recovery_thread;
  int current_attempt = 0;
  std::chrono::milliseconds current_delay{0};
  std::chrono::steady_clock::time_point disconnect_time;

  Impl(RecoveryConfig cfg) : config(std::move(cfg))
  {
    current_delay = config.initial_delay;
  }

  std::chrono::milliseconds calculate_delay()
  {
    switch (config.strategy)
    {
      case ReconnectStrategy::IMMEDIATE:
        return config.initial_delay;

      case ReconnectStrategy::LINEAR_BACKOFF:
        return std::chrono::milliseconds(config.initial_delay.count() * (current_attempt + 1));

      case ReconnectStrategy::EXPONENTIAL_BACKOFF:
      {
        auto delay_ms = static_cast<int64_t>(config.initial_delay.count() *
                                             std::pow(config.backoff_multiplier, current_attempt));
        return std::chrono::milliseconds(std::min(delay_ms, config.max_delay.count()));
      }

      default:
        return std::chrono::milliseconds(0);
    }
  }

  void emit_event(RecoveryEvent event, const std::string& reason = "")
  {
    if (callback)
    {
      callback(event, reason);
    }
  }

  void recovery_loop()
  {
    while (recovering.load() && current_attempt < config.max_attempts)
    {
      current_delay = calculate_delay();

      {
        std::lock_guard lock(mutex);
        state = ConnectionState::RECONNECTING;
      }

      emit_event(RecoveryEvent::RECONNECTING, "Attempt " + std::to_string(current_attempt + 1));

      // Wait for delay
      std::this_thread::sleep_for(current_delay);

      if (!recovering.load()) break;

      // Check if reconnected (would be set externally)
      {
        std::lock_guard lock(mutex);
        if (state == ConnectionState::CONNECTED)
        {
          // Successful reconnection
          stats.reconnect_success++;
          auto now = std::chrono::steady_clock::now();
          auto downtime =
              std::chrono::duration_cast<std::chrono::milliseconds>(now - disconnect_time);
          stats.total_downtime += downtime;

          if (stats.reconnect_success > 0)
          {
            stats.average_recovery_time =
                std::chrono::milliseconds(stats.total_downtime.count() / stats.reconnect_success);
          }

          emit_event(RecoveryEvent::RECONNECTED);
          recovering.store(false);
          return;
        }
      }

      current_attempt++;
    }

    // Failed all attempts
    {
      std::lock_guard lock(mutex);
      state = ConnectionState::FAILED;
      stats.reconnect_failed++;
    }

    emit_event(RecoveryEvent::FAILED, "Max attempts reached");
    recovering.store(false);
  }
};

ConnectionRecovery::ConnectionRecovery(RecoveryConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

ConnectionRecovery::~ConnectionRecovery()
{
  cancel_recovery();
}

void ConnectionRecovery::set_callback(RecoveryCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->callback = std::move(callback);
}

void ConnectionRecovery::on_state_change(ConnectionState new_state)
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->state == ConnectionState::CONNECTED && new_state == ConnectionState::DISCONNECTED)
  {
    impl_->stats.disconnect_count++;
    impl_->disconnect_time = std::chrono::steady_clock::now();
    impl_->emit_event(RecoveryEvent::DISCONNECTED);

    // Start auto recovery if enabled
    if (impl_->config.strategy != ReconnectStrategy::NONE)
    {
      // Trigger recovery asynchronously
    }
  }

  impl_->state = new_state;
}

void ConnectionRecovery::on_network_change(bool has_connectivity)
{
  if (has_connectivity && impl_->config.enable_ice_restart)
  {
    std::lock_guard lock(impl_->mutex);
    impl_->stats.ice_restarts++;
    impl_->emit_event(RecoveryEvent::ICE_RESTART, "Network changed");
  }
}

bool ConnectionRecovery::start_recovery()
{
  if (impl_->config.strategy == ReconnectStrategy::NONE)
  {
    return false;
  }

  if (impl_->recovering.load())
  {
    return false;  // Already recovering
  }

  impl_->current_attempt = 0;
  impl_->recovering.store(true);

  impl_->recovery_thread = std::thread([this]() { impl_->recovery_loop(); });

  return true;
}

void ConnectionRecovery::cancel_recovery()
{
  impl_->recovering.store(false);
  if (impl_->recovery_thread.joinable())
  {
    impl_->recovery_thread.join();
  }
}

void ConnectionRecovery::on_reconnected()
{
  std::lock_guard lock(impl_->mutex);
  impl_->state = ConnectionState::CONNECTED;
}

ConnectionState ConnectionRecovery::state() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->state;
}

RecoveryStats ConnectionRecovery::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

int ConnectionRecovery::current_attempt() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_attempt;
}

std::chrono::milliseconds ConnectionRecovery::next_delay() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_delay;
}

bool ConnectionRecovery::is_recovering() const
{
  return impl_->recovering.load();
}

}  // namespace rtc
