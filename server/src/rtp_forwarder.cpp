/**
 * @file rtp_forwarder.cpp
 * @brief RTP forwarder implementation
 */

#include "rtc/server/rtp_forwarder.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "rtc/udp_socket.h"


namespace rtc
{
namespace server
{

struct PublisherStream
{
  ParticipantId publisher_id;
  StreamId stream_id;
  RtpStreamInfo info;
  std::vector<ForwardingRule> subscribers;
};

struct RtpForwarder::Impl
{
  ForwardCallback forward_callback;
  mutable std::mutex mutex;

  // SSRC -> Publisher stream mapping
  std::unordered_map<uint32_t, PublisherStream> ssrc_to_stream;

  // Publisher ID -> list of SSRCs
  std::unordered_map<ParticipantId, std::vector<uint32_t>> publisher_ssrcs;

  ForwarderStats stats;

  // Scratch buffer for SSRC rewriting
  std::vector<uint8_t> forward_buffer;

  Impl()
  {
    forward_buffer.reserve(1500);  // MTU size
  }

  void forward_packet(const PublisherStream& stream, std::span<const uint8_t> packet)
  {
    for (const auto& rule : stream.subscribers)
    {
      if (!rule.is_active) continue;

      // Check simulcast layer preference
      if (rule.preferred_simulcast_layer >= 0 && stream.info.simulcast_layer >= 0 &&
          stream.info.simulcast_layer != rule.preferred_simulcast_layer)
      {
        continue;  // Skip if not matching layer
      }

      if (forward_callback)
      {
        if (rule.rewritten_ssrc != 0 && rule.rewritten_ssrc != stream.info.ssrc)
        {
          // Rewrite SSRC
          forward_buffer.assign(packet.begin(), packet.end());
          if (forward_buffer.size() >= 12)
          {
            forward_buffer[8] = (rule.rewritten_ssrc >> 24) & 0xFF;
            forward_buffer[9] = (rule.rewritten_ssrc >> 16) & 0xFF;
            forward_buffer[10] = (rule.rewritten_ssrc >> 8) & 0xFF;
            forward_buffer[11] = rule.rewritten_ssrc & 0xFF;
          }
          forward_callback(rule.subscriber_id, forward_buffer, rule.destination);
        }
        else
        {
          // Forward as-is (zero-copy)
          forward_callback(rule.subscriber_id, packet, rule.destination);
        }

        stats.packets_forwarded++;
        stats.bytes_forwarded += packet.size();
      }
    }
  }
};

RtpForwarder::RtpForwarder() : impl_(std::make_unique<Impl>()) {}

RtpForwarder::~RtpForwarder() = default;

void RtpForwarder::set_forward_callback(ForwardCallback callback)
{
  std::lock_guard lock(impl_->mutex);
  impl_->forward_callback = std::move(callback);
}

void RtpForwarder::add_publisher(const ParticipantId& publisher_id, const StreamId& stream_id,
                                 const RtpStreamInfo& info)
{
  std::lock_guard lock(impl_->mutex);

  PublisherStream stream;
  stream.publisher_id = publisher_id;
  stream.stream_id = stream_id;
  stream.info = info;

  impl_->ssrc_to_stream[info.ssrc] = std::move(stream);
  impl_->publisher_ssrcs[publisher_id].push_back(info.ssrc);
  impl_->stats.active_publishers = impl_->publisher_ssrcs.size();
}

void RtpForwarder::remove_publisher(const ParticipantId& publisher_id, const StreamId& stream_id)
{
  std::lock_guard lock(impl_->mutex);

  auto pub_it = impl_->publisher_ssrcs.find(publisher_id);
  if (pub_it == impl_->publisher_ssrcs.end()) return;

  // Find and remove matching SSRCs
  auto& ssrcs = pub_it->second;
  for (auto it = ssrcs.begin(); it != ssrcs.end();)
  {
    auto stream_it = impl_->ssrc_to_stream.find(*it);
    if (stream_it != impl_->ssrc_to_stream.end() && stream_it->second.stream_id == stream_id)
    {
      impl_->ssrc_to_stream.erase(stream_it);
      it = ssrcs.erase(it);
    }
    else
    {
      ++it;
    }
  }

  if (ssrcs.empty())
  {
    impl_->publisher_ssrcs.erase(pub_it);
  }

  impl_->stats.active_publishers = impl_->publisher_ssrcs.size();
}

void RtpForwarder::add_subscription(const ParticipantId& publisher_id,
                                    const ParticipantId& subscriber_id, ForwardingRule rule)
{
  std::lock_guard lock(impl_->mutex);

  rule.subscriber_id = subscriber_id;

  auto pub_it = impl_->publisher_ssrcs.find(publisher_id);
  if (pub_it == impl_->publisher_ssrcs.end()) return;

  // Add rule to all streams from this publisher
  for (uint32_t ssrc : pub_it->second)
  {
    auto stream_it = impl_->ssrc_to_stream.find(ssrc);
    if (stream_it != impl_->ssrc_to_stream.end())
    {
      stream_it->second.subscribers.push_back(rule);
    }
  }

  impl_->stats.active_subscribers++;
}

void RtpForwarder::remove_subscription(const ParticipantId& publisher_id,
                                       const ParticipantId& subscriber_id)
{
  std::lock_guard lock(impl_->mutex);

  auto pub_it = impl_->publisher_ssrcs.find(publisher_id);
  if (pub_it == impl_->publisher_ssrcs.end()) return;

  for (uint32_t ssrc : pub_it->second)
  {
    auto stream_it = impl_->ssrc_to_stream.find(ssrc);
    if (stream_it != impl_->ssrc_to_stream.end())
    {
      auto& subs = stream_it->second.subscribers;
      subs.erase(std::remove_if(subs.begin(), subs.end(), [&](const ForwardingRule& r)
                                { return r.subscriber_id == subscriber_id; }),
                 subs.end());
    }
  }

  impl_->stats.active_subscribers--;
}

void RtpForwarder::set_simulcast_layer(const ParticipantId& publisher_id,
                                       const ParticipantId& subscriber_id, int layer)
{
  std::lock_guard lock(impl_->mutex);

  auto pub_it = impl_->publisher_ssrcs.find(publisher_id);
  if (pub_it == impl_->publisher_ssrcs.end()) return;

  for (uint32_t ssrc : pub_it->second)
  {
    auto stream_it = impl_->ssrc_to_stream.find(ssrc);
    if (stream_it != impl_->ssrc_to_stream.end())
    {
      for (auto& rule : stream_it->second.subscribers)
      {
        if (rule.subscriber_id == subscriber_id)
        {
          rule.preferred_simulcast_layer = layer;
        }
      }
    }
  }
}

void RtpForwarder::on_rtp_packet(uint32_t ssrc, std::span<const uint8_t> packet,
                                 const SocketAddress& /*source*/)
{
  std::lock_guard lock(impl_->mutex);

  impl_->stats.packets_received++;
  impl_->stats.bytes_received += packet.size();

  auto it = impl_->ssrc_to_stream.find(ssrc);
  if (it != impl_->ssrc_to_stream.end())
  {
    impl_->forward_packet(it->second, packet);
  }
  else
  {
    impl_->stats.packets_dropped++;
  }
}

ForwarderStats RtpForwarder::stats() const
{
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

std::vector<ParticipantId> RtpForwarder::get_publishers() const
{
  std::lock_guard lock(impl_->mutex);
  std::vector<ParticipantId> result;
  for (const auto& [id, _] : impl_->publisher_ssrcs)
  {
    result.push_back(id);
  }
  return result;
}

std::vector<ParticipantId> RtpForwarder::get_subscribers(const ParticipantId& publisher_id) const
{
  std::lock_guard lock(impl_->mutex);
  std::vector<ParticipantId> result;

  auto pub_it = impl_->publisher_ssrcs.find(publisher_id);
  if (pub_it == impl_->publisher_ssrcs.end()) return result;

  for (uint32_t ssrc : pub_it->second)
  {
    auto stream_it = impl_->ssrc_to_stream.find(ssrc);
    if (stream_it != impl_->ssrc_to_stream.end())
    {
      for (const auto& rule : stream_it->second.subscribers)
      {
        if (std::find(result.begin(), result.end(), rule.subscriber_id) == result.end())
        {
          result.push_back(rule.subscriber_id);
        }
      }
    }
  }

  return result;
}

}  // namespace server
}  // namespace rtc
