/**
 * @file turn_client.cpp
 * @brief TURN client implementation (stub)
 */

#include "rtc/turn_client.h"

#include "rtc/udp_socket.h"


namespace rtc
{

struct TurnClient::Impl
{
  std::shared_ptr<UdpSocket> socket;
  Config config;
  TurnState state = TurnState::IDLE;
  std::optional<TurnAllocation> allocation;
  TurnDataCallback data_callback;

  Impl(std::shared_ptr<UdpSocket> sock, Config cfg)
      : socket(std::move(sock)), config(std::move(cfg))
  {
  }
};

TurnClient::TurnClient(std::shared_ptr<UdpSocket> socket, Config config)
    : impl_(std::make_unique<Impl>(std::move(socket), std::move(config)))
{
}

TurnClient::~TurnClient() = default;

void TurnClient::allocate(TurnAllocateCallback callback)
{
  impl_->state = TurnState::ALLOCATING;

  // TODO: Send ALLOCATE request
  // For now, simulate failure
  if (callback)
  {
    callback(false, {}, "TURN allocation not implemented");
  }
  impl_->state = TurnState::FAILED;
}

void TurnClient::refresh(TurnAllocateCallback callback)
{
  if (impl_->state != TurnState::ALLOCATED)
  {
    if (callback)
    {
      callback(false, {}, "No active allocation");
    }
    return;
  }

  impl_->state = TurnState::REFRESHING;
  // TODO: Send REFRESH request
  if (callback)
  {
    callback(false, {}, "TURN refresh not implemented");
  }
}

void TurnClient::deallocate()
{
  if (impl_->state == TurnState::ALLOCATED)
  {
    // TODO: Send REFRESH with lifetime=0
    impl_->allocation.reset();
    impl_->state = TurnState::IDLE;
  }
}

void TurnClient::create_permission(const SocketAddress& /*peer_address*/,
                                   TurnPermissionCallback callback)
{
  // TODO: Send CREATE-PERMISSION request
  if (callback)
  {
    callback(false, "TURN permissions not implemented");
  }
}

void TurnClient::bind_channel(const SocketAddress& /*peer_address*/,
                              TurnPermissionCallback callback)
{
  // TODO: Send CHANNEL-BIND request
  if (callback)
  {
    callback(false, "TURN channel binding not implemented");
  }
}

bool TurnClient::send_to(std::span<const uint8_t> /*data*/, const SocketAddress& /*peer_address*/)
{
  if (impl_->state != TurnState::ALLOCATED)
  {
    return false;
  }
  // TODO: Send via Data indication or channel
  return false;
}

void TurnClient::set_data_callback(TurnDataCallback callback)
{
  impl_->data_callback = std::move(callback);
}

bool TurnClient::process_packet(std::span<const uint8_t> /*data*/, const SocketAddress& /*source*/)
{
  // TODO: Process TURN responses and data indications
  return false;
}

TurnState TurnClient::state() const
{
  return impl_->state;
}

std::optional<TurnAllocation> TurnClient::allocation() const
{
  return impl_->allocation;
}

std::optional<SocketAddress> TurnClient::relayed_address() const
{
  if (impl_->allocation)
  {
    return impl_->allocation->relayed_address;
  }
  return std::nullopt;
}

}  // namespace rtc
