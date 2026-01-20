#pragma once

/**
 * @file cluster_coordinator.h
 * @brief Cluster coordination for horizontal scaling
 *
 * Enables multiple SFU/MCU nodes to work together.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtc
{
namespace server
{

using NodeId = std::string;
using RoomId = std::string;

/**
 * @brief Node status in cluster
 */
enum class NodeStatus
{
  JOINING,
  ACTIVE,
  DRAINING,  // Accepting no new connections
  LEAVING,
  OFFLINE,
};

/**
 * @brief Node information
 */
struct ClusterNode
{
  NodeId id;
  std::string address;
  uint16_t port = 0;
  NodeStatus status = NodeStatus::OFFLINE;
  float load_percent = 0.0f;
  size_t active_rooms = 0;
  size_t active_participants = 0;
  std::chrono::steady_clock::time_point last_heartbeat;
};

/**
 * @brief Room location in cluster
 */
struct RoomLocation
{
  RoomId room_id;
  NodeId primary_node;
  std::vector<NodeId> backup_nodes;
};

/**
 * @brief Cluster event type
 */
enum class ClusterEvent
{
  NODE_JOINED,
  NODE_LEFT,
  NODE_FAILED,
  ROOM_CREATED,
  ROOM_MIGRATED,
  LEADER_CHANGED,
};

/**
 * @brief Cluster event callback
 */
using ClusterEventCallback =
    std::function<void(ClusterEvent event, const NodeId& node_id, const std::string& details)>;

/**
 * @brief Cluster configuration
 */
struct ClusterConfig
{
  NodeId node_id;  // This node's ID
  std::string bind_address = "0.0.0.0";
  uint16_t cluster_port = 9000;
  std::vector<std::string> seed_nodes;  // Initial nodes to connect to
  std::chrono::seconds heartbeat_interval{5};
  std::chrono::seconds node_timeout{30};
  bool enable_room_replication = true;
};

/**
 * @brief Cluster coordinator for horizontal scaling
 *
 * Features:
 * - Node discovery and registration
 * - Consistent room-to-node mapping
 * - Load-based room placement
 * - Automatic failover
 * - Leader election
 */
class ClusterCoordinator
{
 public:
  explicit ClusterCoordinator(ClusterConfig config);
  ~ClusterCoordinator();

  // Disable copy
  ClusterCoordinator(const ClusterCoordinator&) = delete;
  ClusterCoordinator& operator=(const ClusterCoordinator&) = delete;

  /**
   * @brief Join the cluster
   */
  bool join();

  /**
   * @brief Leave the cluster gracefully
   */
  void leave();

  /**
   * @brief Set event callback
   */
  void set_event_callback(ClusterEventCallback callback);

  /**
   * @brief Get all nodes in cluster
   */
  [[nodiscard]] std::vector<ClusterNode> get_nodes() const;

  /**
   * @brief Get this node's info
   */
  [[nodiscard]] ClusterNode get_self() const;

  /**
   * @brief Get current leader
   */
  [[nodiscard]] NodeId get_leader() const;

  /**
   * @brief Check if this node is leader
   */
  [[nodiscard]] bool is_leader() const;

  /**
   * @brief Find which node hosts a room
   */
  [[nodiscard]] RoomLocation find_room(const RoomId& room_id) const;

  /**
   * @brief Create room on best node
   * @return Node ID where room was created
   */
  NodeId create_room(const RoomId& room_id);

  /**
   * @brief Report room metrics for load balancing
   */
  void report_room_stats(const RoomId& room_id, size_t participant_count, float bandwidth_mbps);

  /**
   * @brief Update this node's load
   */
  void update_load(float load_percent);

  /**
   * @brief Get node with lowest load
   */
  [[nodiscard]] NodeId get_least_loaded_node() const;

  /**
   * @brief Force leader election
   */
  void trigger_election();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Load balancer for client connections
 */
class LoadBalancer
{
 public:
  /**
   * @brief Get best node for new connection
   * @param client_region Optional client region hint
   * @return Node address to connect to
   */
  static std::string get_best_node(const ClusterCoordinator& cluster,
                                   const std::string& client_region = "");

  /**
   * @brief Get backup nodes for failover
   */
  static std::vector<std::string> get_backup_nodes(const ClusterCoordinator& cluster,
                                                   const NodeId& primary_node, size_t count = 2);
};

}  // namespace server
}  // namespace rtc
