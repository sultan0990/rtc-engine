#pragma once

/**
 * @file metrics_exporter.h
 * @brief Prometheus metrics exporter for monitoring
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace rtc
{

/**
 * @brief Metric type
 */
enum class MetricType
{
  COUNTER,
  GAUGE,
  HISTOGRAM,
  SUMMARY,
};

/**
 * @brief Metric label set
 */
using Labels = std::unordered_map<std::string, std::string>;

/**
 * @brief Metrics exporter configuration
 */
struct MetricsConfig
{
  uint16_t port = 9090;
  std::string path = "/metrics";
  std::string namespace_prefix = "rtc";
  bool enable_default_metrics = true;  // CPU, memory, etc.
};

/**
 * @brief Prometheus-compatible metrics exporter
 *
 * Exports metrics in Prometheus format for monitoring.
 */
class MetricsExporter
{
 public:
  explicit MetricsExporter(MetricsConfig config = {});
  ~MetricsExporter();

  // Disable copy
  MetricsExporter(const MetricsExporter&) = delete;
  MetricsExporter& operator=(const MetricsExporter&) = delete;

  /**
   * @brief Start HTTP metrics server
   */
  bool start();

  /**
   * @brief Stop metrics server
   */
  void stop();

  // Counter operations
  void counter_inc(const std::string& name, const Labels& labels = {}, double value = 1.0);

  // Gauge operations
  void gauge_set(const std::string& name, double value, const Labels& labels = {});
  void gauge_inc(const std::string& name, const Labels& labels = {}, double value = 1.0);
  void gauge_dec(const std::string& name, const Labels& labels = {}, double value = 1.0);

  // Histogram operations
  void histogram_observe(const std::string& name, double value, const Labels& labels = {});

  // Pre-defined RTC metrics
  void record_packet_sent(const std::string& media_type);
  void record_packet_received(const std::string& media_type);
  void record_bytes_sent(size_t bytes, const std::string& media_type);
  void record_bytes_received(size_t bytes, const std::string& media_type);
  void record_latency(double ms, const std::string& operation);
  void record_participant_joined(const std::string& room_id);
  void record_participant_left(const std::string& room_id);
  void set_active_rooms(size_t count);
  void set_active_participants(size_t count);

  /**
   * @brief Get metrics in Prometheus format
   */
  [[nodiscard]] std::string get_metrics() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
