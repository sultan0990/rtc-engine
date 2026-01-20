/**
 * @file stun_client.cpp
 * @brief STUN client implementation (stub)
 */

#include "rtc/stun_client.h"

#include <random>

#include "rtc/udp_socket.h"


namespace rtc
{

// StunTransactionId implementation
bool StunTransactionId::operator==(const StunTransactionId& other) const
{
  return std::memcmp(data, other.data, sizeof(data)) == 0;
}

StunTransactionId StunTransactionId::generate()
{
  StunTransactionId id;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(0, 255);
  for (auto& byte : id.data)
  {
    byte = static_cast<uint8_t>(dist(gen));
  }
  return id;
}

// StunMessage implementation
StunMessage::StunMessage(StunMessageType type) : type_(type)
{
  transaction_id_ = StunTransactionId::generate();
}

std::optional<StunMessage> StunMessage::parse(std::span<const uint8_t> data)
{
  // Minimum STUN header is 20 bytes
  if (data.size() < 20)
  {
    return std::nullopt;
  }

  // Check magic cookie (bytes 4-7 should be 0x2112A442)
  uint32_t magic = (static_cast<uint32_t>(data[4]) << 24) | (static_cast<uint32_t>(data[5]) << 16) |
                   (static_cast<uint32_t>(data[6]) << 8) | static_cast<uint32_t>(data[7]);
  if (magic != 0x2112A442)
  {
    return std::nullopt;
  }

  StunMessage msg;
  msg.type_ = static_cast<StunMessageType>((data[0] << 8) | data[1]);

  // Copy transaction ID
  std::memcpy(msg.transaction_id_.data, &data[8], 12);

  // TODO: Parse attributes
  return msg;
}

std::vector<uint8_t> StunMessage::serialize() const
{
  std::vector<uint8_t> result;
  result.reserve(20);

  // Message type (2 bytes)
  result.push_back(static_cast<uint8_t>(static_cast<uint16_t>(type_) >> 8));
  result.push_back(static_cast<uint8_t>(type_));

  // Message length (2 bytes) - will be updated later
  size_t length_offset = result.size();
  result.push_back(0);
  result.push_back(0);

  // Magic cookie (4 bytes)
  result.push_back(0x21);
  result.push_back(0x12);
  result.push_back(0xA4);
  result.push_back(0x42);

  // Transaction ID (12 bytes)
  result.insert(result.end(), std::begin(transaction_id_.data), std::end(transaction_id_.data));

  // TODO: Serialize attributes

  return result;
}

void StunMessage::add_message_integrity(std::string_view /*password*/)
{
  // TODO: Implement HMAC-SHA1
}

bool StunMessage::verify_message_integrity(std::string_view /*password*/) const
{
  // TODO: Implement
  return true;
}

void StunMessage::add_fingerprint()
{
  // TODO: Implement CRC32
}

std::optional<SocketAddress> StunMessage::get_xor_mapped_address() const
{
  // TODO: Implement
  return std::nullopt;
}

void StunMessage::add_attribute(StunAttribute attr)
{
  attributes_.push_back(std::move(attr));
}

// StunClient implementation
struct StunClient::Impl
{
  std::shared_ptr<UdpSocket> socket;
  Config config;
  StunCallback pending_callback;
  StunTransactionId pending_transaction;
  std::chrono::steady_clock::time_point request_time;

  Impl(std::shared_ptr<UdpSocket> sock, Config cfg)
      : socket(std::move(sock)), config(std::move(cfg))
  {
  }
};

StunClient::StunClient(std::shared_ptr<UdpSocket> socket, Config config)
    : impl_(std::make_unique<Impl>(std::move(socket), std::move(config)))
{
}

StunClient::~StunClient() = default;

void StunClient::get_reflexive_address(StunCallback callback)
{
  impl_->pending_callback = std::move(callback);
  impl_->request_time = std::chrono::steady_clock::now();

  StunMessage request(StunMessageType::BINDING_REQUEST);
  impl_->pending_transaction = request.transaction_id();

  auto data = request.serialize();

  // Send to first STUN server
  if (!impl_->config.servers.empty())
  {
    // Parse server address
    auto server = impl_->config.servers[0];
    size_t colon = server.find(':');
    std::string host = server.substr(0, colon);
    uint16_t port = 3478;
    if (colon != std::string::npos)
    {
      port = static_cast<uint16_t>(std::stoi(server.substr(colon + 1)));
    }

    impl_->socket->send_to(data, {host, port});
  }
}

StunResult StunClient::get_reflexive_address_sync()
{
  // TODO: Implement synchronous version
  StunResult result;
  result.success = false;
  result.error_message = "Not implemented";
  return result;
}

bool StunClient::process_packet(std::span<const uint8_t> data, const SocketAddress& /*source*/)
{
  auto msg = StunMessage::parse(data);
  if (!msg)
  {
    return false;
  }

  if (msg->transaction_id() == impl_->pending_transaction)
  {
    StunResult result;
    if (msg->type() == StunMessageType::BINDING_RESPONSE)
    {
      result.success = true;
      auto addr = msg->get_xor_mapped_address();
      if (addr)
      {
        result.reflexive_address = *addr;
      }
      result.rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - impl_->request_time);
    }
    else
    {
      result.success = false;
      result.error_message = "Binding error response";
    }

    if (impl_->pending_callback)
    {
      impl_->pending_callback(result);
      impl_->pending_callback = nullptr;
    }
    return true;
  }

  return false;
}

}  // namespace rtc
