/**
 * @file subscription_manager.cpp
 * @brief Subscription manager implementation
 */

#include "rtc/server/subscription_manager.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace rtc
{
namespace server
{

struct SubscriptionKey
{
  ParticipantId subscriber_id;
  ParticipantId publisher_id;
  StreamId stream_id;

  bool operator==(const SubscriptionKey& other) const
  {
    return subscriber_id == other.subscriber_id && publisher_id == other.publisher_id &&
           stream_id == other.stream_id;
  }
};

struct SubscriptionKeyHash
{
  size_t operator()(const SubscriptionKey& k) const
  {
    size_t h1 = std::hash<std::string>{}(k.subscriber_id);
    size_t h2 = std::hash<std::string>{}(k.publisher_id);
    size_t h3 = std::hash<std::string>{}(k.stream_id);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

struct StreamKey
{
  ParticipantId publisher_id;
  StreamId stream_id;

  bool operator==(const StreamKey& other) const
  {
    return publisher_id == other.publisher_id && stream_id == other.stream_id;
  }
};

struct StreamKeyHash
{
  size_t operator()(const StreamKey& k) const
  {
    size_t h1 = std::hash<std::string>{}(k.publisher_id);
    size_t h2 = std::hash<std::string>{}(k.stream_id);
    return h1 ^ (h2 << 1);
  }
};

struct SubscriptionManager::Impl
{
  mutable std::mutex mutex;
  LayerSwitchCallback layer_switch_callback;

  // All subscriptions
  std::unordered_map<SubscriptionKey, Subscription, SubscriptionKeyHash> subscriptions;

  // Available layers per stream
  std::unordered_map<StreamKey, std::vector<SimulcastLayerInfo>, StreamKeyHash> stream_layers;

  // Bandwidth info per subscriber
  std::unordered_map<ParticipantId, BandwidthInfo> bandwidth_info;

  int select_best_layer(const ParticipantId& subscriber_id, const StreamKey& stream_key) const
  {
    auto bw_it = bandwidth_info.find(subscriber_id);
    if (bw_it == bandwidth_info.end())
    {
      return 2;  // Default to highest if no bandwidth info
    }

    auto layer_it = stream_layers.find(stream_key);
    if (layer_it == stream_layers.end())
    {
      return 0;
    }

    // Find highest layer that fits bandwidth
    int best_layer = 0;
    uint64_t available_bps = bw_it->second.estimated_bps;

    for (const auto& layer : layer_it->second)
    {
      if (layer.is_active && static_cast<uint64_t>(layer.bitrate_kbps * 1000) <= available_bps)
      {
        best_layer = layer.layer_index;
      }
    }

    return best_layer;
  }
};

SubscriptionManager::SubscriptionManager() : impl_(std::make_unique<Impl>()) {}

SubscriptionManager::~SubscriptionManager() = default;

void SubscriptionManager::set_layer_switch_callback(LayerSwitchCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->layer_switch_callback = std::move(callback);
}

void SubscriptionManager::set_available_layers(const ParticipantId& publisher_id,
                                               const StreamId& stream_id,
                                               const std::vector<SimulcastLayerInfo>& layers)
{
  std::lock_guard lock(impl_->mutex);
  impl_->stream_layers[{publisher_id, stream_id}] = layers;
}

void SubscriptionManager::subscribe(const ParticipantId& subscriber_id,
                                    const ParticipantId& publisher_id, const StreamId& stream_id,
                                    int target_layer)
{
  std::lock_guard lock(impl_->mutex);

  SubscriptionKey key{subscriber_id, publisher_id, stream_id};

  Subscription sub;
  sub.publisher_id = publisher_id;
  sub.stream_id = stream_id;
  sub.target_layer = target_layer;
  sub.current_layer = target_layer >= 0 ? target_layer : 2;
  sub.is_paused = false;

  impl_->subscriptions[key] = std::move(sub);
}

void SubscriptionManager::unsubscribe(const ParticipantId& subscriber_id,
                                      const ParticipantId& publisher_id, const StreamId& stream_id)
{
  std::lock_guard lock(impl_->mutex);
  impl_->subscriptions.erase({subscriber_id, publisher_id, stream_id});
}

void SubscriptionManager::set_paused(const ParticipantId& subscriber_id,
                                     const ParticipantId& publisher_id, bool paused)
{
  std::lock_guard lock(impl_->mutex);

  for (auto& [key, sub] : impl_->subscriptions)
  {
    if (key.subscriber_id == subscriber_id && key.publisher_id == publisher_id)
    {
      sub.is_paused = paused;
    }
  }
}

void SubscriptionManager::set_target_layer(const ParticipantId& subscriber_id,
                                           const ParticipantId& publisher_id, int layer)
{
  std::lock_guard lock(impl_->mutex);

  for (auto& [key, sub] : impl_->subscriptions)
  {
    if (key.subscriber_id == subscriber_id && key.publisher_id == publisher_id)
    {
      sub.target_layer = layer;
    }
  }
}

void SubscriptionManager::update_bandwidth(const ParticipantId& subscriber_id,
                                           const BandwidthInfo& info)
{
  std::lock_guard lock(impl_->mutex);
  impl_->bandwidth_info[subscriber_id] = info;
}

void SubscriptionManager::process()
{
  std::lock_guard lock(impl_->mutex);

  for (auto& [key, sub] : impl_->subscriptions)
  {
    if (sub.is_paused || sub.target_layer >= 0)
    {
      continue;  // Don't auto-adjust if paused or manually set
    }

    int best_layer = impl_->select_best_layer(key.subscriber_id, {key.publisher_id, key.stream_id});

    if (best_layer != sub.current_layer)
    {
      int old_layer = sub.current_layer;
      sub.current_layer = best_layer;

      if (impl_->layer_switch_callback)
      {
        impl_->layer_switch_callback(key.subscriber_id, key.publisher_id, old_layer, best_layer);
      }
    }
  }
}

int SubscriptionManager::get_current_layer(const ParticipantId& subscriber_id,
                                           const ParticipantId& publisher_id) const
{
  std::lock_guard lock(impl_->mutex);

  for (const auto& [key, sub] : impl_->subscriptions)
  {
    if (key.subscriber_id == subscriber_id && key.publisher_id == publisher_id)
    {
      return sub.current_layer;
    }
  }
  return -1;
}

std::vector<Subscription> SubscriptionManager::get_subscriptions(
    const ParticipantId& subscriber_id) const
{
  std::lock_guard lock(impl_->mutex);

  std::vector<Subscription> result;
  for (const auto& [key, sub] : impl_->subscriptions)
  {
    if (key.subscriber_id == subscriber_id)
    {
      result.push_back(sub);
    }
  }
  return result;
}

size_t SubscriptionManager::subscription_count() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->subscriptions.size();
}

}  // namespace server
}  // namespace rtc
