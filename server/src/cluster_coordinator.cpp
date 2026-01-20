/**
 * @file cluster_coordinator.cpp
 * @brief Cluster coordinator implementation
 */

#include "rtc/server/cluster_coordinator.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

namespace rtc
{
namespace server
{

struct ClusterCoordinator::Impl
{
  ClusterConfig config;
  mutable std::mutex mutex;

  std::unordered_map<NodeId, ClusterNode> nodes;
  std::unordered_map<RoomId, RoomLocation> rooms;
  ClusterEventCallback event_callback;

  NodeId leader_id;
  std::atomic<bool> running{false};
  std::thread heartbeat_thread;
  ClusterNode self_node;

  Impl(ClusterConfig cfg) : config(std::move(cfg))
  {
    self_node.id = config.node_id;
    self_node.address = config.bind_address;
    self_node.port = config.cluster_port;
    self_node.status = NodeStatus::OFFLINE;
  }

  void emit_event(ClusterEvent event, const NodeId& node_id, const std::string& details = "")
  {
    if (event_callback)
    {
      event_callback(event, node_id, details);
    }
  }

  NodeId elect_leader()
  {
    // Simple lexicographic leader election
    std::lock_guard lock(mutex);

    std::vector<NodeId> active_nodes;
    for (const auto& [id, node] : nodes)
    {
      if (node.status == NodeStatus::ACTIVE)
      {
        active_nodes.push_back(id);
      }
    }

    if (active_nodes.empty())
    {
      return config.node_id;  // Self is leader if alone
    }

    std::sort(active_nodes.begin(), active_nodes.end());
    return active_nodes.front();
  }

  void heartbeat_loop()
  {
    while (running.load())
    {
      auto now = std::chrono::steady_clock::now();

      {
        std::lock_guard lock(mutex);

        // Update self
        self_node.last_heartbeat = now;

        // Check for dead nodes
        for (auto it = nodes.begin(); it != nodes.end();)
        {
          auto age =
              std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_heartbeat);

          if (age > config.node_timeout)
          {
            emit_event(ClusterEvent::NODE_FAILED, it->first);

            // Re-elect leader if needed
            if (it->first == leader_id)
            {
              leader_id = elect_leader();
              emit_event(ClusterEvent::LEADER_CHANGED, leader_id);
            }

            it = nodes.erase(it);
          }
          else
          {
            ++it;
          }
        }
      }

      // TODO: Send heartbeat to other nodes

      std::this_thread::sleep_for(config.heartbeat_interval);
    }
  }
};

ClusterCoordinator::ClusterCoordinator(ClusterConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

ClusterCoordinator::~ClusterCoordinator()
{
  leave();
}

bool ClusterCoordinator::join()
{
  if (impl_->running.load()) return false;

  impl_->self_node.status = NodeStatus::ACTIVE;
  impl_->self_node.last_heartbeat = std::chrono::steady_clock::now();

  {
    std::lock_guard lock(impl_->mutex);
    impl_->nodes[impl_->config.node_id] = impl_->self_node;
    impl_->leader_id = impl_->elect_leader();
  }

  impl_->running.store(true);
  impl_->heartbeat_thread = std::thread([this]() { impl_->heartbeat_loop(); });

  impl_->emit_event(ClusterEvent::NODE_JOINED, impl_->config.node_id);

  return true;
}

void ClusterCoordinator::leave()
{
  if (!impl_->running.load()) return;

  {
    std::lock_guard lock(impl_->mutex);
    impl_->self_node.status = NodeStatus::LEAVING;
  }

  impl_->emit_event(ClusterEvent::NODE_LEFT, impl_->config.node_id);

  impl_->running.store(false);
  if (impl_->heartbeat_thread.joinable())
  {
    impl_->heartbeat_thread.join();
  }

  {
    std::lock_guard lock(impl_->mutex);
    impl_->nodes.erase(impl_->config.node_id);
  }
}

void ClusterCoordinator::set_event_callback(ClusterEventCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->event_callback = std::move(callback);
}

std::vector<ClusterNode> ClusterCoordinator::get_nodes() const
{
  std::lock_guard lock(impl_->mutex);
  std::vector<ClusterNode> result;
  for (const auto& [_, node] : impl_->nodes)
  {
    result.push_back(node);
  }
  return result;
}

ClusterNode ClusterCoordinator::get_self() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->self_node;
}

NodeId ClusterCoordinator::get_leader() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->leader_id;
}

bool ClusterCoordinator::is_leader() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->leader_id == impl_->config.node_id;
}

RoomLocation ClusterCoordinator::find_room(const RoomId& room_id) const
{
  std::lock_guard lock(impl_->mutex);
  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    return it->second;
  }
  return {};
}

NodeId ClusterCoordinator::create_room(const RoomId& room_id)
{
  std::lock_guard lock(impl_->mutex);

  NodeId best_node = get_least_loaded_node();

  RoomLocation location;
  location.room_id = room_id;
  location.primary_node = best_node;

  impl_->rooms[room_id] = location;

  impl_->emit_event(ClusterEvent::ROOM_CREATED, best_node, room_id);

  return best_node;
}

void ClusterCoordinator::report_room_stats(const RoomId& room_id, size_t participant_count,
                                           float /*bandwidth_mbps*/)
{
  std::lock_guard lock(impl_->mutex);
  (void)room_id;
  (void)participant_count;
  // Update room stats for load balancing decisions
}

void ClusterCoordinator::update_load(float load_percent)
{
  std::lock_guard lock(impl_->mutex);
  impl_->self_node.load_percent = load_percent;
  impl_->nodes[impl_->config.node_id].load_percent = load_percent;
}

NodeId ClusterCoordinator::get_least_loaded_node() const
{
  std::lock_guard lock(impl_->mutex);

  NodeId best;
  float min_load = 100.0f;

  for (const auto& [id, node] : impl_->nodes)
  {
    if (node.status == NodeStatus::ACTIVE && node.load_percent < min_load)
    {
      min_load = node.load_percent;
      best = id;
    }
  }

  return best.empty() ? impl_->config.node_id : best;
}

void ClusterCoordinator::trigger_election()
{
  std::lock_guard lock(impl_->mutex);
  NodeId new_leader = impl_->elect_leader();
  if (new_leader != impl_->leader_id)
  {
    impl_->leader_id = new_leader;
    impl_->emit_event(ClusterEvent::LEADER_CHANGED, new_leader);
  }
}

// LoadBalancer implementation
std::string LoadBalancer::get_best_node(const ClusterCoordinator& cluster,
                                        const std::string& /*client_region*/)
{
  auto node_id = cluster.get_least_loaded_node();
  auto nodes = cluster.get_nodes();

  for (const auto& node : nodes)
  {
    if (node.id == node_id)
    {
      return node.address + ":" + std::to_string(node.port);
    }
  }

  return "";
}

std::vector<std::string> LoadBalancer::get_backup_nodes(const ClusterCoordinator& cluster,
                                                        const NodeId& primary_node, size_t count)
{
  std::vector<std::string> backups;
  auto nodes = cluster.get_nodes();

  for (const auto& node : nodes)
  {
    if (node.id != primary_node && node.status == NodeStatus::ACTIVE)
    {
      backups.push_back(node.address + ":" + std::to_string(node.port));
      if (backups.size() >= count) break;
    }
  }

  return backups;
}

}  // namespace server
}  // namespace rtc
