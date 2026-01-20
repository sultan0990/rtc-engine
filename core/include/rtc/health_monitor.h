#pragma once

/**
 * @file health_monitor.h
 * @brief Health monitoring and connection recovery
 *
 * Monitors server and connection health with automatic recovery.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtc
{

/**
 * @brief Health status levels
 */
enum class HealthStatus
{
  HEALTHY,
  DEGRADED,
  UNHEALTHY,
  CRITICAL,
};

/**
 * @brief Component health info
 */
struct ComponentHealth
{
  std::string name;
  HealthStatus status = HealthStatus::HEALTHY;
  std::string message;
  std::chrono::steady_clock::time_point last_check;
  std::chrono::milliseconds latency{0};
  float load_percent = 0.0f;
};

/**
 * @brief System health summary
 */
struct SystemHealth
{
  HealthStatus overall_status = HealthStatus::HEALTHY;
  std::vector<ComponentHealth> components;
  float cpu_usage_percent = 0.0f;
  float memory_usage_percent = 0.0f;
  size_t active_connections = 0;
  std::chrono::seconds uptime{0};
};

/**
 * @brief Health check callback
 */
using HealthCheckCallback = std::function<ComponentHealth()>;

/**
 * @brief Health change callback
 */
using HealthChangeCallback = std::function<void(const SystemHealth&)>;

/**
 * @brief Health monitor configuration
 */
struct HealthMonitorConfig
{
  std::chrono::seconds check_interval{5};
  std::chrono::seconds unhealthy_threshold{30};
  float cpu_warning_threshold = 80.0f;
  float cpu_critical_threshold = 95.0f;
  float memory_warning_threshold = 80.0f;
  float memory_critical_threshold = 95.0f;
  bool enable_auto_recovery = true;
};

/**
 * @brief Health monitoring system
 *
 * Monitors:
 * - CPU and memory usage
 * - Network connectivity
 * - Component latencies
 * - Connection states
 */
class HealthMonitor
{
 public:
  explicit HealthMonitor(HealthMonitorConfig config = {});
  ~HealthMonitor();

  // Disable copy
  HealthMonitor(const HealthMonitor&) = delete;
  HealthMonitor& operator=(const HealthMonitor&) = delete;

  /**
   * @brief Start monitoring
   */
  void start();

  /**
   * @brief Stop monitoring
   */
  void stop();

  /**
   * @brief Register a component health check
   */
  void register_component(const std::string& name, HealthCheckCallback check);

  /**
   * @brief Unregister a component
   */
  void unregister_component(const std::string& name);

  /**
   * @brief Set callback for health changes
   */
  void set_health_callback(HealthChangeCallback callback);

  /**
   * @brief Get current system health
   */
  [[nodiscard]] SystemHealth get_health() const;

  /**
   * @brief Get specific component health
   */
  [[nodiscard]] ComponentHealth get_component_health(const std::string& name) const;

  /**
   * @brief Force immediate health check
   */
  void check_now();

  /**
   * @brief Check if system is healthy
   */
  [[nodiscard]] bool is_healthy() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
