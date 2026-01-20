/**
 * @file ice_agent.cpp
 * @brief ICE agent implementation (stub)
 */

#include "rtc/ice_agent.h"

#include <random>
#include <sstream>

#include "rtc/stun_client.h"
#include "rtc/turn_client.h"
#include "rtc/udp_socket.h"


namespace rtc
{

// IceCandidate implementation
uint32_t IceCandidate::calculate_priority(IceCandidateType type, uint32_t local_preference,
                                          uint32_t component)
{
  // Priority = (2^24) * type_preference + (2^8) * local_preference + (256 - component)
  uint32_t type_pref = 0;
  switch (type)
  {
    case IceCandidateType::HOST:
      type_pref = 126;
      break;
    case IceCandidateType::PEER_REFLEXIVE:
      type_pref = 110;
      break;
    case IceCandidateType::SERVER_REFLEXIVE:
      type_pref = 100;
      break;
    case IceCandidateType::RELAY:
      type_pref = 0;
      break;
  }
  return (type_pref << 24) + (local_preference << 8) + (256 - component);
}

std::string IceCandidate::to_sdp() const
{
  std::ostringstream ss;
  ss << "candidate:" << foundation << " " << component << " " << protocol << " " << priority << " "
     << address.ip << " " << address.port << " typ ";

  switch (type)
  {
    case IceCandidateType::HOST:
      ss << "host";
      break;
    case IceCandidateType::SERVER_REFLEXIVE:
      ss << "srflx";
      break;
    case IceCandidateType::PEER_REFLEXIVE:
      ss << "prflx";
      break;
    case IceCandidateType::RELAY:
      ss << "relay";
      break;
  }

  if (type != IceCandidateType::HOST && !related_address.ip.empty())
  {
    ss << " raddr " << related_address.ip << " rport " << related_address.port;
  }

  return ss.str();
}

std::optional<IceCandidate> IceCandidate::from_sdp(std::string_view /*sdp*/)
{
  // TODO: Implement SDP parsing
  return std::nullopt;
}

// IceCredentials implementation
IceCredentials IceCredentials::generate()
{
  IceCredentials creds;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(0, 35);

  const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";

  // Generate ufrag (4-256 chars, we use 8)
  for (int i = 0; i < 8; ++i)
  {
    creds.username_fragment += chars[dist(gen)];
  }

  // Generate password (22-256 chars, we use 24)
  for (int i = 0; i < 24; ++i)
  {
    creds.password += chars[dist(gen)];
  }

  return creds;
}

// IceAgent implementation
struct IceAgent::Impl
{
  Config config;
  IceAgentCallbacks callbacks;
  IceCredentials local_credentials;
  IceCredentials remote_credentials;
  std::vector<IceCandidate> local_candidates;
  std::vector<IceCandidate> remote_candidates;
  std::vector<IceCandidatePair> candidate_pairs;
  std::optional<IceCandidatePair> selected_pair;
  IceConnectionState connection_state = IceConnectionState::NEW;
  IceGatheringState gathering_state = IceGatheringState::NEW;
  std::shared_ptr<UdpSocket> socket;
  std::unique_ptr<StunClient> stun_client;
  Stats stats;

  Impl(Config cfg) : config(std::move(cfg))
  {
    local_credentials = IceCredentials::generate();
    socket = UdpSocket::create();
    if (socket)
    {
      socket->bind("0.0.0.0", 0);
    }
  }
};

IceAgent::IceAgent(Config config) : impl_(std::make_unique<Impl>(std::move(config))) {}

IceAgent::~IceAgent() = default;

void IceAgent::set_callbacks(IceAgentCallbacks callbacks)
{
  impl_->callbacks = std::move(callbacks);
}

const IceCredentials& IceAgent::local_credentials() const
{
  return impl_->local_credentials;
}

void IceAgent::set_remote_credentials(const IceCredentials& credentials)
{
  impl_->remote_credentials = credentials;
}

void IceAgent::gather_candidates()
{
  impl_->gathering_state = IceGatheringState::GATHERING;
  if (impl_->callbacks.on_gathering_state_change)
  {
    impl_->callbacks.on_gathering_state_change(impl_->gathering_state);
  }

  // Gather host candidates
  if (impl_->config.gather_host_candidates && impl_->socket)
  {
    IceCandidate host;
    host.foundation = "1";
    host.component = 1;
    host.protocol = "udp";
    host.address = impl_->socket->local_address();
    host.type = IceCandidateType::HOST;
    host.priority = IceCandidate::calculate_priority(IceCandidateType::HOST, 65535, 1);

    impl_->local_candidates.push_back(host);
    impl_->stats.candidates_gathered++;

    if (impl_->callbacks.on_candidate)
    {
      impl_->callbacks.on_candidate(host);
    }
  }

  // TODO: Gather server-reflexive candidates via STUN
  // TODO: Gather relay candidates via TURN

  impl_->gathering_state = IceGatheringState::COMPLETE;
  if (impl_->callbacks.on_gathering_state_change)
  {
    impl_->callbacks.on_gathering_state_change(impl_->gathering_state);
  }
}

void IceAgent::add_remote_candidate(const IceCandidate& candidate)
{
  impl_->remote_candidates.push_back(candidate);

  // Create pairs with all local candidates
  for (const auto& local : impl_->local_candidates)
  {
    IceCandidatePair pair;
    pair.local = local;
    pair.remote = candidate;
    // Priority calculation for controlling/controlled
    if (impl_->config.role == IceRole::CONTROLLING)
    {
      pair.priority = (static_cast<uint64_t>(local.priority) << 32) + candidate.priority;
    }
    else
    {
      pair.priority = (static_cast<uint64_t>(candidate.priority) << 32) + local.priority;
    }
    pair.state = IceCandidatePair::State::FROZEN;
    impl_->candidate_pairs.push_back(pair);
  }
}

void IceAgent::set_remote_candidates_complete()
{
  // Start connectivity checks
  impl_->connection_state = IceConnectionState::CHECKING;
  if (impl_->callbacks.on_connection_state_change)
  {
    impl_->callbacks.on_connection_state_change(impl_->connection_state);
  }
}

const std::vector<IceCandidate>& IceAgent::local_candidates() const
{
  return impl_->local_candidates;
}

IceConnectionState IceAgent::connection_state() const
{
  return impl_->connection_state;
}

IceGatheringState IceAgent::gathering_state() const
{
  return impl_->gathering_state;
}

std::optional<IceCandidatePair> IceAgent::selected_pair() const
{
  return impl_->selected_pair;
}

bool IceAgent::send(std::span<const uint8_t> data)
{
  if (!impl_->selected_pair || impl_->connection_state != IceConnectionState::CONNECTED)
  {
    return false;
  }

  auto [error, sent] = impl_->socket->send_to(data, impl_->selected_pair->remote.address);
  if (!error)
  {
    impl_->selected_pair->bytes_sent += sent;
  }
  return !error;
}

bool IceAgent::process_packet(std::span<const uint8_t> data, const SocketAddress& source)
{
  // Check if it's a STUN message (first 2 bits should be 00)
  if (data.size() >= 20 && (data[0] & 0xC0) == 0x00)
  {
    // It's a STUN packet
    // TODO: Process STUN binding request/response
    impl_->stats.connectivity_checks_received++;
    return true;
  }

  // It's application data
  if (impl_->callbacks.on_data)
  {
    impl_->callbacks.on_data(data, source);
  }
  return true;
}

void IceAgent::process()
{
  // TODO: Implement periodic processing
  // - Send connectivity checks
  // - Handle retransmissions
  // - Update pair states
}

void IceAgent::close()
{
  impl_->connection_state = IceConnectionState::CLOSED;
  if (impl_->callbacks.on_connection_state_change)
  {
    impl_->callbacks.on_connection_state_change(impl_->connection_state);
  }
  if (impl_->socket)
  {
    impl_->socket->close();
  }
}

IceAgent::Stats IceAgent::stats() const
{
  return impl_->stats;
}

}  // namespace rtc
