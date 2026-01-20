#pragma once

/**
 * @file ice_agent.h
 * @brief ICE (Interactive Connectivity Establishment) agent
 *
 * Implements RFC 8445 ICE for establishing peer-to-peer connections.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtc
{

struct SocketAddress;
class UdpSocket;
class StunClient;
class TurnClient;

/**
 * @brief ICE candidate type
 */
enum class IceCandidateType
{
  HOST,              // Local interface address
  SERVER_REFLEXIVE,  // Address discovered via STUN
  PEER_REFLEXIVE,    // Address discovered during connectivity checks
  RELAY,             // TURN relay address
};

/**
 * @brief ICE candidate
 */
struct IceCandidate
{
  std::string foundation;        // Unique identifier for candidate
  uint32_t component = 1;        // Component ID (1=RTP, 2=RTCP)
  std::string protocol = "udp";  // Transport protocol
  uint32_t priority = 0;         // Candidate priority
  SocketAddress address;         // Candidate address
  IceCandidateType type = IceCandidateType::HOST;
  SocketAddress related_address;  // Related address (for srflx/relay)

  /**
   * @brief Calculate priority based on type and component
   */
  [[nodiscard]] static uint32_t calculate_priority(IceCandidateType type, uint32_t local_preference,
                                                   uint32_t component);

  /**
   * @brief Convert to SDP attribute string
   */
  [[nodiscard]] std::string to_sdp() const;

  /**
   * @brief Parse from SDP attribute string
   */
  [[nodiscard]] static std::optional<IceCandidate> from_sdp(std::string_view sdp);
};

/**
 * @brief ICE candidate pair
 */
struct IceCandidatePair
{
  IceCandidate local;
  IceCandidate remote;
  uint64_t priority = 0;

  enum class State
  {
    FROZEN,
    WAITING,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
  };
  State state = State::FROZEN;

  // Stats
  std::chrono::milliseconds rtt{0};
  size_t bytes_sent = 0;
  size_t bytes_received = 0;
};

/**
 * @brief ICE connection state
 */
enum class IceConnectionState
{
  NEW,
  CHECKING,
  CONNECTED,
  COMPLETED,
  FAILED,
  DISCONNECTED,
  CLOSED,
};

/**
 * @brief ICE gathering state
 */
enum class IceGatheringState
{
  NEW,
  GATHERING,
  COMPLETE,
};

/**
 * @brief ICE role
 */
enum class IceRole
{
  CONTROLLING,
  CONTROLLED,
};

/**
 * @brief ICE credentials
 */
struct IceCredentials
{
  std::string username_fragment;  // ufrag
  std::string password;           // pwd

  [[nodiscard]] static IceCredentials generate();
};

/**
 * @brief ICE agent callbacks
 */
struct IceAgentCallbacks
{
  std::function<void(const IceCandidate&)> on_candidate;
  std::function<void(IceGatheringState)> on_gathering_state_change;
  std::function<void(IceConnectionState)> on_connection_state_change;
  std::function<void(const IceCandidatePair&)> on_selected_pair;
  std::function<void(std::span<const uint8_t>, const SocketAddress&)> on_data;
};

/**
 * @brief ICE agent for establishing connections
 */
class IceAgent
{
 public:
  /**
   * @brief Configuration
   */
  struct Config
  {
    IceRole role = IceRole::CONTROLLING;
    std::vector<std::string> stun_servers = {
        "stun.l.google.com:19302",
    };
    struct TurnServer
    {
      std::string uri;
      std::string username;
      std::string password;
    };
    std::vector<TurnServer> turn_servers;

    // Timeouts
    std::chrono::milliseconds connectivity_check_interval{50};
    std::chrono::milliseconds keepalive_interval{15000};
    std::chrono::seconds nomination_timeout{10};

    // Candidate type preferences
    bool gather_host_candidates = true;
    bool gather_srflx_candidates = true;
    bool gather_relay_candidates = true;
  };

  explicit IceAgent(Config config = {});
  ~IceAgent();

  /**
   * @brief Set callbacks
   */
  void set_callbacks(IceAgentCallbacks callbacks);

  /**
   * @brief Get local credentials
   */
  [[nodiscard]] const IceCredentials& local_credentials() const;

  /**
   * @brief Set remote credentials
   */
  void set_remote_credentials(const IceCredentials& credentials);

  /**
   * @brief Start gathering candidates
   */
  void gather_candidates();

  /**
   * @brief Add remote candidate
   */
  void add_remote_candidate(const IceCandidate& candidate);

  /**
   * @brief Signal end of remote candidates
   */
  void set_remote_candidates_complete();

  /**
   * @brief Get local candidates
   */
  [[nodiscard]] const std::vector<IceCandidate>& local_candidates() const;

  /**
   * @brief Get current connection state
   */
  [[nodiscard]] IceConnectionState connection_state() const;

  /**
   * @brief Get current gathering state
   */
  [[nodiscard]] IceGatheringState gathering_state() const;

  /**
   * @brief Get selected candidate pair
   */
  [[nodiscard]] std::optional<IceCandidatePair> selected_pair() const;

  /**
   * @brief Send data over selected pair
   * @param data Data to send
   * @return True if sent, false if no connection
   */
  bool send(std::span<const uint8_t> data);

  /**
   * @brief Process incoming packet
   * @param data Raw packet data
   * @param source Source address
   * @return True if packet was handled (STUN/data)
   */
  bool process_packet(std::span<const uint8_t> data, const SocketAddress& source);

  /**
   * @brief Periodic processing (call from event loop)
   */
  void process();

  /**
   * @brief Close the agent
   */
  void close();

  /**
   * @brief Get statistics
   */
  struct Stats
  {
    size_t candidates_gathered = 0;
    size_t connectivity_checks_sent = 0;
    size_t connectivity_checks_received = 0;
    std::chrono::milliseconds time_to_connected{0};
  };
  [[nodiscard]] Stats stats() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rtc
