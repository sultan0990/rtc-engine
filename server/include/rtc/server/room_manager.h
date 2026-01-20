#pragma once

/**
 * @file room_manager.h
 * @brief Multi-room management for SFU
 *
 * Manages conference rooms, participants, and their media streams.
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtc
{

struct SocketAddress;

namespace server
{

/**
 * @brief Room identifier
 */
using RoomId = std::string;

/**
 * @brief Participant identifier
 */
using ParticipantId = std::string;

/**
 * @brief Participant role
 */
enum class ParticipantRole
{
  HOST,
  MODERATOR,
  PRESENTER,
  ATTENDEE,
};

/**
 * @brief Participant media state
 */
struct MediaState
{
  bool audio_enabled = true;
  bool video_enabled = true;
  bool screen_share_enabled = false;
  bool audio_muted = false;
  bool video_muted = false;
};

/**
 * @brief Participant information
 */
struct Participant
{
  ParticipantId id;
  std::string display_name;
  ParticipantRole role = ParticipantRole::ATTENDEE;
  MediaState media_state;
  SocketAddress address;
  std::chrono::steady_clock::time_point join_time;
  bool is_connected = true;
};

/**
 * @brief Room configuration
 */
struct RoomConfig
{
  size_t max_participants = 100;
  bool allow_audio = true;
  bool allow_video = true;
  bool allow_screen_share = true;
  bool require_password = false;
  std::string password;
  std::chrono::minutes auto_close_after{60};  // Close if empty for
};

/**
 * @brief Room information
 */
struct Room
{
  RoomId id;
  std::string name;
  RoomConfig config;
  std::vector<Participant> participants;
  std::chrono::steady_clock::time_point created_at;
  bool is_locked = false;
};

/**
 * @brief Room statistics
 */
struct RoomStats
{
  size_t participant_count = 0;
  size_t audio_streams = 0;
  size_t video_streams = 0;
  uint64_t total_bytes_received = 0;
  uint64_t total_bytes_sent = 0;
  std::chrono::seconds uptime{0};
};

/**
 * @brief Room event type
 */
enum class RoomEvent
{
  PARTICIPANT_JOINED,
  PARTICIPANT_LEFT,
  MEDIA_STATE_CHANGED,
  ROOM_LOCKED,
  ROOM_UNLOCKED,
  ROOM_CLOSED,
};

/**
 * @brief Room event callback
 */
using RoomEventCallback = std::function<void(const RoomId& room_id, RoomEvent event,
                                             const ParticipantId& participant_id)>;

/**
 * @brief Room manager for multi-room conferences
 */
class RoomManager
{
 public:
  RoomManager();
  ~RoomManager();

  // Disable copy
  RoomManager(const RoomManager&) = delete;
  RoomManager& operator=(const RoomManager&) = delete;

  /**
   * @brief Set event callback
   */
  void set_event_callback(RoomEventCallback callback);

  /**
   * @brief Create a new room
   * @param room_id Room identifier
   * @param name Display name
   * @param config Room configuration
   * @return True if created
   */
  bool create_room(const RoomId& room_id, const std::string& name, RoomConfig config = {});

  /**
   * @brief Close a room
   */
  void close_room(const RoomId& room_id);

  /**
   * @brief Lock/unlock a room
   */
  void set_room_locked(const RoomId& room_id, bool locked);

  /**
   * @brief Get room by ID
   */
  [[nodiscard]] std::optional<Room> get_room(const RoomId& room_id) const;

  /**
   * @brief Get all rooms
   */
  [[nodiscard]] std::vector<Room> get_all_rooms() const;

  /**
   * @brief Add participant to room
   * @param room_id Room to join
   * @param participant Participant info
   * @param password Optional password
   * @return True if joined successfully
   */
  bool join_room(const RoomId& room_id, const Participant& participant,
                 const std::string& password = "");

  /**
   * @brief Remove participant from room
   */
  void leave_room(const RoomId& room_id, const ParticipantId& participant_id);

  /**
   * @brief Update participant media state
   */
  void update_media_state(const RoomId& room_id, const ParticipantId& participant_id,
                          const MediaState& state);

  /**
   * @brief Get participants in a room
   */
  [[nodiscard]] std::vector<Participant> get_participants(const RoomId& room_id) const;

  /**
   * @brief Get room statistics
   */
  [[nodiscard]] RoomStats get_room_stats(const RoomId& room_id) const;

  /**
   * @brief Periodic cleanup (remove empty/expired rooms)
   */
  void cleanup();

  /**
   * @brief Get total room count
   */
  [[nodiscard]] size_t room_count() const;

  /**
   * @brief Get total participant count across all rooms
   */
  [[nodiscard]] size_t total_participants() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace server
}  // namespace rtc
