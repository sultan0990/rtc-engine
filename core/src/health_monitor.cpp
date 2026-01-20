/**
 * @file health_monitor.cpp
 * @brief Health monitor implementation
 */

#include "rtc/health_monitor.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef __linux__
#include <fstream>
#include <sstream>
#endif

namespace rtc
{

struct HealthMonitor::Impl
{
  HealthMonitorConfig config;
  mutable std::mutex mutex;

  std::unordered_map<std::string, HealthCheckCallback> components;
  std::unordered_map<std::string, ComponentHealth> component_health;
  HealthChangeCallback health_callback;

  std::atomic<bool> running{false};
  std::thread monitor_thread;
  std::chrono::steady_clock::time_point start_time;
  SystemHealth current_health;

  Impl(HealthMonitorConfig cfg) : config(std::move(cfg))
  {
    start_time = std::chrono::steady_clock::now();
  }

  float get_cpu_usage()
  {
#ifdef __linux__
    static long prev_idle = 0, prev_total = 0;

    std::ifstream stat("/proc/stat");
    std::string cpu_line;
    std::getline(stat, cpu_line);

    std::istringstream iss(cpu_line);
    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

    long total = user + nice + system + idle + iowait + irq + softirq;
    long idle_time = idle + iowait;

    long diff_total = total - prev_total;
    long diff_idle = idle_time - prev_idle;

    prev_total = total;
    prev_idle = idle_time;

    if (diff_total == 0) return 0.0f;
    return 100.0f * (1.0f - static_cast<float>(diff_idle) / diff_total);
#else
    return 0.0f;  // Windows implementation would use GetSystemTimes
#endif
  }

  float get_memory_usage()
  {
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    long mem_total = 0, mem_available = 0;

    while (std::getline(meminfo, line))
    {
      if (line.find("MemTotal:") == 0)
      {
        std::istringstream iss(line.substr(9));
        iss >> mem_total;
      }
      else if (line.find("MemAvailable:") == 0)
      {
        std::istringstream iss(line.substr(13));
        iss >> mem_available;
      }
    }

    if (mem_total == 0) return 0.0f;
    return 100.0f * (1.0f - static_cast<float>(mem_available) / mem_total);
#else
    return 0.0f;
#endif
  }

  HealthStatus determine_overall_status()
  {
    if (current_health.cpu_usage_percent >= config.cpu_critical_threshold ||
        current_health.memory_usage_percent >= config.memory_critical_threshold)
    {
      return HealthStatus::CRITICAL;
    }

    bool has_unhealthy = false;
    bool has_degraded = false;

    for (const auto& [_, health] : component_health)
    {
      if (health.status == HealthStatus::CRITICAL)
      {
        return HealthStatus::CRITICAL;
      }
      if (health.status == HealthStatus::UNHEALTHY)
      {
        has_unhealthy = true;
      }
      else if (health.status == HealthStatus::DEGRADED)
      {
        has_degraded = true;
      }
    }

    if (has_unhealthy) return HealthStatus::UNHEALTHY;
    if (has_degraded) return HealthStatus::DEGRADED;

    if (current_health.cpu_usage_percent >= config.cpu_warning_threshold ||
        current_health.memory_usage_percent >= config.memory_warning_threshold)
    {
      return HealthStatus::DEGRADED;
    }

    return HealthStatus::HEALTHY;
  }

  void check_loop()
  {
    while (running.load())
    {
      {
        std::lock_guard lock(mutex);

        current_health.cpu_usage_percent = get_cpu_usage();
        current_health.memory_usage_percent = get_memory_usage();

        auto now = std::chrono::steady_clock::now();
        current_health.uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);

        // Check all components
        for (const auto& [name, check] : components)
        {
          auto health = check();
          health.name = name;
          health.last_check = now;
          component_health[name] = health;
        }

        // Build component list
        current_health.components.clear();
        for (const auto& [_, health] : component_health)
        {
          current_health.components.push_back(health);
        }

        // Determine overall status
        auto prev_status = current_health.overall_status;
        current_health.overall_status = determine_overall_status();

        // Notify on change
        if (current_health.overall_status != prev_status && health_callback)
        {
          health_callback(current_health);
        }
      }

      std::this_thread::sleep_for(config.check_interval);
    }
  }
};

HealthMonitor::HealthMonitor(HealthMonitorConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

HealthMonitor::~HealthMonitor()
{
  stop();
}

void HealthMonitor::start()
{
  if (impl_->running.load()) return;

  impl_->running.store(true);
  impl_->monitor_thread = std::thread([this]() { impl_->check_loop(); });
}

void HealthMonitor::stop()
{
  if (!impl_->running.load()) return;

  impl_->running.store(false);
  if (impl_->monitor_thread.joinable())
  {
    impl_->monitor_thread.join();
  }
}

void HealthMonitor::register_component(const std::string& name, HealthCheckCallback check)
{
  std::lock_guard lock(impl_->mutex);
  impl_->components[name] = std::move(check);
}

void HealthMonitor::unregister_component(const std::string& name)
{
  std::lock_guard lock(impl_->mutex);
  impl_->components.erase(name);
  impl_->component_health.erase(name);
}

void HealthMonitor::set_health_callback(HealthChangeCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->health_callback = std::move(callback);
}

SystemHealth HealthMonitor::get_health() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_health;
}

ComponentHealth HealthMonitor::get_component_health(const std::string& name) const
{
  std::lock_guard lock(impl_->mutex);
  auto it = impl_->component_health.find(name);
  if (it != impl_->component_health.end())
  {
    return it->second;
  }
  return {};
}

void HealthMonitor::check_now()
{
  // Trigger immediate check
  std::lock_guard lock(impl_->mutex);

  auto now = std::chrono::steady_clock::now();
  for (const auto& [name, check] : impl_->components)
  {
    auto health = check();
    health.name = name;
    health.last_check = now;
    impl_->component_health[name] = health;
  }
}

bool HealthMonitor::is_healthy() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_health.overall_status == HealthStatus::HEALTHY ||
         impl_->current_health.overall_status == HealthStatus::DEGRADED;
}

}  // namespace rtc
