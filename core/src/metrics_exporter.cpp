/**
 * @file metrics_exporter.cpp
 * @brief Prometheus metrics exporter implementation
 */

#include "rtc/metrics_exporter.h"

#include <atomic>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace rtc
{

struct MetricValue
{
  MetricType type = MetricType::COUNTER;
  std::atomic<double> value{0.0};
  std::string help;
};

struct LabeledMetricKey
{
  std::string name;
  Labels labels;

  bool operator==(const LabeledMetricKey& other) const
  {
    return name == other.name && labels == other.labels;
  }
};

struct LabeledMetricKeyHash
{
  size_t operator()(const LabeledMetricKey& k) const
  {
    size_t h = std::hash<std::string>{}(k.name);
    for (const auto& [key, val] : k.labels)
    {
      h ^= std::hash<std::string>{}(key) ^ std::hash<std::string>{}(val);
    }
    return h;
  }
};

struct MetricsExporter::Impl
{
  MetricsConfig config;
  mutable std::mutex mutex;

  std::unordered_map<LabeledMetricKey, MetricValue, LabeledMetricKeyHash> metrics;
  std::atomic<bool> running{false};

  Impl(MetricsConfig cfg) : config(std::move(cfg)) {}

  std::string labels_to_string(const Labels& labels) const
  {
    if (labels.empty()) return "";

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, val] : labels)
    {
      if (!first) oss << ",";
      oss << key << "=\"" << val << "\"";
      first = false;
    }
    oss << "}";
    return oss.str();
  }

  std::string format_metric(const std::string& name, double value, const Labels& labels) const
  {
    std::ostringstream oss;
    oss << config.namespace_prefix << "_" << name << labels_to_string(labels) << " " << value
        << "\n";
    return oss.str();
  }
};

MetricsExporter::MetricsExporter(MetricsConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

MetricsExporter::~MetricsExporter()
{
  stop();
}

bool MetricsExporter::start()
{
  impl_->running.store(true);
  // TODO: Start HTTP server on config.port
  return true;
}

void MetricsExporter::stop()
{
  impl_->running.store(false);
}

void MetricsExporter::counter_inc(const std::string& name, const Labels& labels, double value)
{
  LabeledMetricKey key{name, labels};
  std::lock_guard lock(impl_->mutex);

  auto& metric = impl_->metrics[key];
  metric.type = MetricType::COUNTER;
  metric.value.store(metric.value.load() + value);
}

void MetricsExporter::gauge_set(const std::string& name, double value, const Labels& labels)
{
  LabeledMetricKey key{name, labels};
  std::lock_guard lock(impl_->mutex);

  auto& metric = impl_->metrics[key];
  metric.type = MetricType::GAUGE;
  metric.value.store(value);
}

void MetricsExporter::gauge_inc(const std::string& name, const Labels& labels, double value)
{
  LabeledMetricKey key{name, labels};
  std::lock_guard lock(impl_->mutex);

  auto& metric = impl_->metrics[key];
  metric.type = MetricType::GAUGE;
  metric.value.store(metric.value.load() + value);
}

void MetricsExporter::gauge_dec(const std::string& name, const Labels& labels, double value)
{
  gauge_inc(name, labels, -value);
}

void MetricsExporter::histogram_observe(const std::string& name, double value, const Labels& labels)
{
  // Simplified: just store as gauge for now
  gauge_set(name, value, labels);
}

void MetricsExporter::record_packet_sent(const std::string& media_type)
{
  counter_inc("packets_sent_total", {{"type", media_type}});
}

void MetricsExporter::record_packet_received(const std::string& media_type)
{
  counter_inc("packets_received_total", {{"type", media_type}});
}

void MetricsExporter::record_bytes_sent(size_t bytes, const std::string& media_type)
{
  counter_inc("bytes_sent_total", {{"type", media_type}}, static_cast<double>(bytes));
}

void MetricsExporter::record_bytes_received(size_t bytes, const std::string& media_type)
{
  counter_inc("bytes_received_total", {{"type", media_type}}, static_cast<double>(bytes));
}

void MetricsExporter::record_latency(double ms, const std::string& operation)
{
  histogram_observe("latency_ms", ms, {{"operation", operation}});
}

void MetricsExporter::record_participant_joined(const std::string& room_id)
{
  counter_inc("participant_joins_total", {{"room", room_id}});
}

void MetricsExporter::record_participant_left(const std::string& room_id)
{
  counter_inc("participant_leaves_total", {{"room", room_id}});
}

void MetricsExporter::set_active_rooms(size_t count)
{
  gauge_set("active_rooms", static_cast<double>(count));
}

void MetricsExporter::set_active_participants(size_t count)
{
  gauge_set("active_participants", static_cast<double>(count));
}

std::string MetricsExporter::get_metrics() const
{
  std::lock_guard lock(impl_->mutex);
  std::ostringstream oss;

  for (const auto& [key, metric] : impl_->metrics)
  {
    oss << impl_->format_metric(key.name, metric.value.load(), key.labels);
  }

  return oss.str();
}

}  // namespace rtc
