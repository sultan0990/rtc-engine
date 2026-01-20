/**
 * @file bitrate_controller.cpp
 * @brief Adaptive bitrate controller implementation
 */

#include "rtc/video/bitrate_controller.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace rtc
{
namespace video
{

struct BitrateController::Impl
{
  BitrateControllerConfig config;
  BitrateCallback callback;
  mutable std::mutex mutex;

  uint64_t current_bitrate = 0;
  uint64_t target_bitrate = 0;
  float current_loss = 0.0f;
  float current_rtt = 0.0f;
  bool overusing = false;

  std::chrono::steady_clock::time_point last_update;
  uint64_t bytes_sent_since_update = 0;

  Impl(BitrateControllerConfig cfg)
      : config(std::move(cfg)),
        current_bitrate(cfg.start_bitrate_bps),
        target_bitrate(cfg.start_bitrate_bps),
        last_update(std::chrono::steady_clock::now())
  {
  }

  void update_bitrate()
  {
    uint64_t new_bitrate = target_bitrate;

    // Decrease if loss is high
    if (current_loss > config.loss_threshold)
    {
      new_bitrate = static_cast<uint64_t>(current_bitrate * config.decrease_rate);
      overusing = true;
    }
    // Increase if no issues
    else if (!overusing)
    {
      new_bitrate = static_cast<uint64_t>(current_bitrate * config.increase_rate);
    }
    else
    {
      // Recovery phase - increase slowly
      new_bitrate = static_cast<uint64_t>(current_bitrate * 1.02);
      if (current_loss < 0.005f)
      {
        overusing = false;
      }
    }

    // Clamp to min/max
    new_bitrate = std::clamp(new_bitrate, config.min_bitrate_bps, config.max_bitrate_bps);

    // Also respect REMB
    new_bitrate = std::min(new_bitrate, target_bitrate);

    if (new_bitrate != current_bitrate)
    {
      current_bitrate = new_bitrate;
      if (callback)
      {
        callback(current_bitrate);
      }
    }
  }
};

BitrateController::BitrateController(BitrateControllerConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

BitrateController::~BitrateController() = default;

void BitrateController::set_callback(BitrateCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->callback = std::move(callback);
}

void BitrateController::on_remb(uint64_t bitrate_bps)
{
  std::lock_guard lock(impl_->mutex);
  impl_->target_bitrate = bitrate_bps;
  impl_->update_bitrate();
}

void BitrateController::on_packet_loss(float loss_rate)
{
  std::lock_guard lock(impl_->mutex);
  impl_->current_loss = loss_rate;
}

void BitrateController::on_rtt(float rtt_ms)
{
  std::lock_guard lock(impl_->mutex);
  impl_->current_rtt = rtt_ms;
}

void BitrateController::on_packet_sent(size_t size_bytes)
{
  std::lock_guard lock(impl_->mutex);
  impl_->bytes_sent_since_update += size_bytes;
}

void BitrateController::process()
{
  std::lock_guard lock(impl_->mutex);

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - impl_->last_update);

  if (elapsed.count() >= 25)  // Update every 25ms
  {
    impl_->update_bitrate();
    impl_->last_update = now;
    impl_->bytes_sent_since_update = 0;
  }
}

BitrateEstimate BitrateController::estimate() const
{
  std::lock_guard lock(impl_->mutex);

  BitrateEstimate est;
  est.estimated_bitrate_bps = impl_->current_bitrate;
  est.target_bitrate_bps = impl_->target_bitrate;
  est.packet_loss = impl_->current_loss;
  est.rtt_ms = impl_->current_rtt;
  est.is_overusing = impl_->overusing;
  est.is_underusing = impl_->current_bitrate < impl_->target_bitrate * 0.8;

  return est;
}

uint64_t BitrateController::target_bitrate() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->current_bitrate;
}

void BitrateController::set_bitrate(uint64_t bitrate_bps)
{
  std::lock_guard lock(impl_->mutex);
  impl_->current_bitrate =
      std::clamp(bitrate_bps, impl_->config.min_bitrate_bps, impl_->config.max_bitrate_bps);
}

// SimulcastController
std::vector<SimulcastLayer> SimulcastController::get_default_layers(int width, int height,
                                                                    int max_bitrate_kbps)
{
  std::vector<SimulcastLayer> layers;

  // High (full resolution)
  SimulcastLayer high;
  high.width = width;
  high.height = height;
  high.fps = 30;
  high.bitrate_kbps = max_bitrate_kbps * 60 / 100;  // 60% of total
  high.active = true;
  layers.push_back(high);

  // Medium (half resolution)
  SimulcastLayer mid;
  mid.width = width / 2;
  mid.height = height / 2;
  mid.fps = 30;
  mid.bitrate_kbps = max_bitrate_kbps * 30 / 100;  // 30% of total
  mid.active = true;
  layers.push_back(mid);

  // Low (quarter resolution)
  SimulcastLayer low;
  low.width = width / 4;
  low.height = height / 4;
  low.fps = 15;
  low.bitrate_kbps = max_bitrate_kbps * 10 / 100;  // 10% of total
  low.active = true;
  layers.push_back(low);

  return layers;
}

std::vector<SimulcastLayer> SimulcastController::select_layers(
    const std::vector<SimulcastLayer>& layers, int available_bitrate_kbps)
{
  std::vector<SimulcastLayer> active;

  int remaining = available_bitrate_kbps;
  for (const auto& layer : layers)
  {
    if (layer.bitrate_kbps <= remaining)
    {
      active.push_back(layer);
      remaining -= layer.bitrate_kbps;
    }
  }

  // Always include at least one layer
  if (active.empty() && !layers.empty())
  {
    active.push_back(layers.back());  // Lowest quality
  }

  return active;
}

}  // namespace video
}  // namespace rtc
