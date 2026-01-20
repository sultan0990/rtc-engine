#pragma once

/**
 * @file subscription_manager.h
 * @brief Subscription and simulcast layer management
 *
 * Manages which streams each participant receives
 * and handles simulcast layer selection based on bandwidth.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtc
{
namespace server
{

using ParticipantId = std::string;
using StreamId = std::string;

/**
 * @brief Simulcast layer info
 */
struct SimulcastLayerInfo
{
  int layer_index = 0;  // 0=low, 1=mid, 2=high
  int width = 0;
  int height = 0;
  int fps = 0;
  int bitrate_kbps = 0;
  bool is_active = false;
};

/**
 * @brief Stream subscription
 */
struct Subscription
{
  ParticipantId publisher_id;
  StreamId stream_id;
  int target_layer = -1;  // Auto-select if -1
  int current_layer = 0;
  bool is_paused = false;
  uint64_t bytes_received = 0;
};

/**
 * @brief Subscriber bandwidth info (from REMB)
 */
struct BandwidthInfo
{
  uint64_t estimated_bps = 0;
  float packet_loss = 0.0f;
  float rtt_ms = 0.0f;
};

/**
 * @brief Layer switch callback
 */
using LayerSwitchCallback =
    std::function<void(const ParticipantId& subscriber_id, const ParticipantId& publisher_id,
                       int old_layer, int new_layer)>;

/**
 * @brief Subscription manager for simulcast layer selection
 */
class SubscriptionManager
{
 public:
  SubscriptionManager();
  ~SubscriptionManager();

  // Disable copy
  SubscriptionManager(const SubscriptionManager&) = delete;
  SubscriptionManager& operator=(const SubscriptionManager&) = delete;

  /**
   * @brief Set layer switch callback
   */
  void set_layer_switch_callback(LayerSwitchCallback callback);

  /**
   * @brief Register available layers for a publisher stream
   */
  void set_available_layers(const ParticipantId& publisher_id, const StreamId& stream_id,
                            const std::vector<SimulcastLayerInfo>& layers);

  /**
   * @brief Add a subscription
   */
  void subscribe(const ParticipantId& subscriber_id, const ParticipantId& publisher_id,
                 const StreamId& stream_id, int target_layer = -1);

  /**
   * @brief Remove a subscription
   */
  void unsubscribe(const ParticipantId& subscriber_id, const ParticipantId& publisher_id,
                   const StreamId& stream_id);

  /**
   * @brief Pause/resume a subscription
   */
  void set_paused(const ParticipantId& subscriber_id, const ParticipantId& publisher_id,
                  bool paused);

  /**
   * @brief Set preferred layer for a subscription
   */
  void set_target_layer(const ParticipantId& subscriber_id, const ParticipantId& publisher_id,
                        int layer);

  /**
   * @brief Update subscriber bandwidth info (from REMB)
   */
  void update_bandwidth(const ParticipantId& subscriber_id, const BandwidthInfo& info);

  /**
   * @brief Process layer selections (call periodically)
   * This automatically adjusts layers based on bandwidth
   */
  void process();

  /**
   * @brief Get current layer for a subscription
   */
  [[nodiscard]] int get_current_layer(const ParticipantId& subscriber_id,
                                      const ParticipantId& publisher_id) const;

  /**
   * @brief Get all subscriptions for a subscriber
   */
  [[nodiscard]] std::vector<Subscription> get_subscriptions(
      const ParticipantId& subscriber_id) const;

  /**
   * @brief Get total subscription count
   */
  [[nodiscard]] size_t subscription_count() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
