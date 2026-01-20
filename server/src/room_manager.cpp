/**
 * @file room_manager.cpp
 * @brief Room manager implementation
 */

#include "rtc/server/room_manager.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace rtc
{
namespace server
{

struct RoomManager::Impl
{
  mutable std::mutex mutex;
  std::unordered_map<RoomId, Room> rooms;
  RoomEventCallback event_callback;

  void emit_event(const RoomId& room_id, RoomEvent event, const ParticipantId& participant_id = "")
  {
    if (event_callback)
    {
      event_callback(room_id, event, participant_id);
    }
  }
};

RoomManager::RoomManager() : impl_(std::make_unique<Impl>()) {}

RoomManager::~RoomManager() = default;

void RoomManager::set_event_callback(RoomEventCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->event_callback = std::move(callback);
}

bool RoomManager::create_room(const RoomId& room_id, const std::string& name, RoomConfig config)
{
  std::lock_guard lock(impl_->mutex);

  if (impl_->rooms.count(room_id) > 0)
  {
    return false;  // Room exists
  }

  Room room;
  room.id = room_id;
  room.name = name;
  room.config = std::move(config);
  room.created_at = std::chrono::steady_clock::now();

  impl_->rooms[room_id] = std::move(room);
  return true;
}

void RoomManager::close_room(const RoomId& room_id)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    impl_->emit_event(room_id, RoomEvent::ROOM_CLOSED);
    impl_->rooms.erase(it);
  }
}

void RoomManager::set_room_locked(const RoomId& room_id, bool locked)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    it->second.is_locked = locked;
    impl_->emit_event(room_id, locked ? RoomEvent::ROOM_LOCKED : RoomEvent::ROOM_UNLOCKED);
  }
}

std::optional<Room> RoomManager::get_room(const RoomId& room_id) const
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Room> RoomManager::get_all_rooms() const
{
  std::lock_guard lock(impl_->mutex);

  std::vector<Room> result;
  result.reserve(impl_->rooms.size());
  for (const auto& [_, room] : impl_->rooms)
  {
    result.push_back(room);
  }
  return result;
}

bool RoomManager::join_room(const RoomId& room_id, const Participant& participant,
                            const std::string& password)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it == impl_->rooms.end())
  {
    return false;  // Room not found
  }

  auto& room = it->second;

  // Check if locked
  if (room.is_locked)
  {
    return false;
  }

  // Check max participants
  if (room.participants.size() >= room.config.max_participants)
  {
    return false;
  }

  // Check password
  if (room.config.require_password && room.config.password != password)
  {
    return false;
  }

  // Check if already joined
  for (const auto& p : room.participants)
  {
    if (p.id == participant.id)
    {
      return false;
    }
  }

  // Add participant
  Participant p = participant;
  p.join_time = std::chrono::steady_clock::now();
  p.is_connected = true;
  room.participants.push_back(std::move(p));

  impl_->emit_event(room_id, RoomEvent::PARTICIPANT_JOINED, participant.id);
  return true;
}

void RoomManager::leave_room(const RoomId& room_id, const ParticipantId& participant_id)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it == impl_->rooms.end()) return;

  auto& participants = it->second.participants;
  auto prev_size = participants.size();

  participants.erase(std::remove_if(participants.begin(), participants.end(),
                                    [&](const Participant& p) { return p.id == participant_id; }),
                     participants.end());

  if (participants.size() < prev_size)
  {
    impl_->emit_event(room_id, RoomEvent::PARTICIPANT_LEFT, participant_id);
  }
}

void RoomManager::update_media_state(const RoomId& room_id, const ParticipantId& participant_id,
                                     const MediaState& state)
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it == impl_->rooms.end()) return;

  for (auto& p : it->second.participants)
  {
    if (p.id == participant_id)
    {
      p.media_state = state;
      impl_->emit_event(room_id, RoomEvent::MEDIA_STATE_CHANGED, participant_id);
      break;
    }
  }
}

std::vector<Participant> RoomManager::get_participants(const RoomId& room_id) const
{
  std::lock_guard lock(impl_->mutex);

  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    return it->second.participants;
  }
  return {};
}

RoomStats RoomManager::get_room_stats(const RoomId& room_id) const
{
  std::lock_guard lock(impl_->mutex);

  RoomStats stats;

  auto it = impl_->rooms.find(room_id);
  if (it != impl_->rooms.end())
  {
    const auto& room = it->second;
    stats.participant_count = room.participants.size();

    for (const auto& p : room.participants)
    {
      if (p.media_state.audio_enabled) stats.audio_streams++;
      if (p.media_state.video_enabled) stats.video_streams++;
    }

    auto now = std::chrono::steady_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(now - room.created_at);
  }

  return stats;
}

void RoomManager::cleanup()
{
  std::lock_guard lock(impl_->mutex);

  auto now = std::chrono::steady_clock::now();

  for (auto it = impl_->rooms.begin(); it != impl_->rooms.end();)
  {
    auto& room = it->second;

    // Remove if empty for too long
    if (room.participants.empty())
    {
      auto age = std::chrono::duration_cast<std::chrono::minutes>(now - room.created_at);
      if (age >= room.config.auto_close_after)
      {
        impl_->emit_event(room.id, RoomEvent::ROOM_CLOSED);
        it = impl_->rooms.erase(it);
        continue;
      }
    }

    ++it;
  }
}

size_t RoomManager::room_count() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->rooms.size();
}

size_t RoomManager::total_participants() const
{
  std::lock_guard lock(impl_->mutex);

  size_t total = 0;
  for (const auto& [_, room] : impl_->rooms)
  {
    total += room.participants.size();
  }
  return total;
}

}  // namespace server
}  // namespace rtc
