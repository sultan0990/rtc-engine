#pragma once

/**
 * @file bitrate_controller.h
 * @brief Adaptive bitrate control using RTCP feedback
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace rtc
{
namespace video
{

/**
 * @brief Bitrate estimation result
 */
struct BitrateEstimate
{
  uint64_t estimated_bitrate_bps = 0;
  uint64_t target_bitrate_bps = 0;
  float packet_loss = 0.0f;
  float rtt_ms = 0.0f;
  bool is_overusing = false;
  bool is_underusing = false;
};

/**
 * @brief Callback when bitrate should change
 */
using BitrateCallback = std::function<void(uint64_t new_bitrate_bps)>;

/**
 * @brief Bitrate controller configuration
 */
struct BitrateControllerConfig
{
  uint64_t start_bitrate_bps = 1'000'000;  // 1 Mbps start
  uint64_t min_bitrate_bps = 100'000;      // 100 Kbps min
  uint64_t max_bitrate_bps = 5'000'000;    // 5 Mbps max
  float increase_rate = 1.08f;             // 8% increase per RTT
  float decrease_rate = 0.85f;             // 15% decrease on loss
  float loss_threshold = 0.02f;            // 2% loss triggers decrease
};

/**
 * @brief Adaptive bitrate controller
 *
 * Implements Google Congestion Control (GCC) style algorithm:
 * - Uses RTCP REMB for receiver-estimated max bitrate
 * - Adjusts based on packet loss and RTT
 * - Probe-based bandwidth estimation
 */
class BitrateController
{
 public:
  explicit BitrateController(BitrateControllerConfig config = {});
  ~BitrateController();

  // Disable copy
  BitrateController(const BitrateController&) = delete;
  BitrateController& operator=(const BitrateController&) = delete;

  /**
   * @brief Set callback for bitrate changes
   */
  void set_callback(BitrateCallback callback);

  /**
   * @brief Process REMB feedback from receiver
   * @param bitrate_bps Receiver's estimated max bitrate
   */
  void on_remb(uint64_t bitrate_bps);

  /**
   * @brief Update with packet loss information
   * @param loss_rate Packet loss rate (0.0 - 1.0)
   */
  void on_packet_loss(float loss_rate);

  /**
   * @brief Update RTT
   * @param rtt_ms Round-trip time in milliseconds
   */
  void on_rtt(float rtt_ms);

  /**
   * @brief Called when a packet is sent
   * @param size_bytes Packet size
   */
  void on_packet_sent(size_t size_bytes);

  /**
   * @brief Periodic update (call every ~25ms)
   */
  void process();

  /**
   * @brief Get current bitrate estimate
   */
  [[nodiscard]] BitrateEstimate estimate() const;

  /**
   * @brief Get current target bitrate
   */
  [[nodiscard]] uint64_t target_bitrate() const;

  /**
   * @brief Force a specific bitrate (for testing)
   */
  void set_bitrate(uint64_t bitrate_bps);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Simulcast layer configuration
 */
struct SimulcastLayer
{
  int width = 0;
  int height = 0;
  int fps = 0;
  int bitrate_kbps = 0;
  bool active = true;
};

/**
 * @brief Simulcast controller for multi-resolution encoding
 */
class SimulcastController
{
 public:
  /**
   * @brief Get recommended simulcast layers for a resolution
   * @param width Full resolution width
   * @param height Full resolution height
   * @param max_bitrate_kbps Maximum total bitrate
   * @return List of simulcast layers (high, medium, low)
   */
  [[nodiscard]] static std::vector<SimulcastLayer> get_default_layers(int width, int height,
                                                                      int max_bitrate_kbps);

  /**
   * @brief Select which layers to send based on available bandwidth
   * @param layers Available layers
   * @param available_bitrate_kbps Current bandwidth
   * @return Active layers
   */
  [[nodiscard]] static std::vector<SimulcastLayer> select_layers(
      const std::vector<SimulcastLayer>& layers, int available_bitrate_kbps);
};

}  // namespace video
}  // namespace rtc
