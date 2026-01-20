/**
 * @file rtp_packet.cpp
 * @brief RTP packet implementation
 */

#include "rtc/rtp_packet.h"

#include <cstring>

namespace rtc
{

namespace
{

// Network byte order helpers
inline uint16_t read_uint16_be(const uint8_t* data)
{
  return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

inline uint32_t read_uint32_be(const uint8_t* data)
{
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

inline void write_uint16_be(uint8_t* data, uint16_t value)
{
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value);
}

inline void write_uint32_be(uint8_t* data, uint32_t value)
{
  data[0] = static_cast<uint8_t>(value >> 24);
  data[1] = static_cast<uint8_t>(value >> 16);
  data[2] = static_cast<uint8_t>(value >> 8);
  data[3] = static_cast<uint8_t>(value);
}

}  // namespace

std::optional<RtpPacket> RtpPacket::parse(std::span<const uint8_t> data)
{
  if (data.size() < RtpHeader::MIN_SIZE)
  {
    return std::nullopt;
  }

  RtpPacket packet;
  auto& header = packet.header_;

  // First byte: V(2) P(1) X(1) CC(4)
  uint8_t byte0 = data[0];
  header.version = (byte0 >> 6) & 0x03;
  header.padding = (byte0 >> 5) & 0x01;
  header.extension = (byte0 >> 4) & 0x01;
  header.csrc_count = byte0 & 0x0F;

  // Validate version
  if (header.version != 2)
  {
    return std::nullopt;
  }

  // Second byte: M(1) PT(7)
  uint8_t byte1 = data[1];
  header.marker = (byte1 >> 7) & 0x01;
  header.payload_type = byte1 & 0x7F;

  // Sequence number (2 bytes)
  header.sequence = read_uint16_be(&data[2]);

  // Timestamp (4 bytes)
  header.timestamp = read_uint32_be(&data[4]);

  // SSRC (4 bytes)
  header.ssrc = read_uint32_be(&data[8]);

  size_t offset = RtpHeader::MIN_SIZE;

  // CSRC list
  if (header.csrc_count > 0)
  {
    size_t csrc_size = header.csrc_count * 4;
    if (data.size() < offset + csrc_size)
    {
      return std::nullopt;
    }
    header.csrc.resize(header.csrc_count);
    for (int i = 0; i < header.csrc_count; ++i)
    {
      header.csrc[i] = read_uint32_be(&data[offset + i * 4]);
    }
    offset += csrc_size;
  }

  // Extension header
  if (header.extension)
  {
    if (data.size() < offset + 4)
    {
      return std::nullopt;
    }
    RtpExtension ext;
    ext.profile = read_uint16_be(&data[offset]);
    uint16_t ext_length = read_uint16_be(&data[offset + 2]) * 4;
    offset += 4;

    if (data.size() < offset + ext_length)
    {
      return std::nullopt;
    }
    ext.data.assign(data.begin() + offset, data.begin() + offset + ext_length);
    packet.extension_ = std::move(ext);
    offset += ext_length;
  }

  // Payload
  if (offset < data.size())
  {
    size_t payload_size = data.size() - offset;

    // Handle padding
    if (header.padding && payload_size > 0)
    {
      uint8_t padding_length = data[data.size() - 1];
      if (padding_length <= payload_size)
      {
        payload_size -= padding_length;
      }
    }

    packet.payload_.assign(data.begin() + offset, data.begin() + offset + payload_size);
  }

  return packet;
}

std::vector<uint8_t> RtpPacket::serialize() const
{
  std::vector<uint8_t> result;
  result.reserve(size());

  // First byte
  uint8_t byte0 = (header_.version << 6) | (header_.padding ? 0x20 : 0) |
                  (extension_.has_value() ? 0x10 : 0) | (header_.csrc_count & 0x0F);
  result.push_back(byte0);

  // Second byte
  uint8_t byte1 = (header_.marker ? 0x80 : 0) | (header_.payload_type & 0x7F);
  result.push_back(byte1);

  // Sequence number
  result.push_back(static_cast<uint8_t>(header_.sequence >> 8));
  result.push_back(static_cast<uint8_t>(header_.sequence));

  // Timestamp
  result.push_back(static_cast<uint8_t>(header_.timestamp >> 24));
  result.push_back(static_cast<uint8_t>(header_.timestamp >> 16));
  result.push_back(static_cast<uint8_t>(header_.timestamp >> 8));
  result.push_back(static_cast<uint8_t>(header_.timestamp));

  // SSRC
  result.push_back(static_cast<uint8_t>(header_.ssrc >> 24));
  result.push_back(static_cast<uint8_t>(header_.ssrc >> 16));
  result.push_back(static_cast<uint8_t>(header_.ssrc >> 8));
  result.push_back(static_cast<uint8_t>(header_.ssrc));

  // CSRC
  for (uint32_t csrc : header_.csrc)
  {
    result.push_back(static_cast<uint8_t>(csrc >> 24));
    result.push_back(static_cast<uint8_t>(csrc >> 16));
    result.push_back(static_cast<uint8_t>(csrc >> 8));
    result.push_back(static_cast<uint8_t>(csrc));
  }

  // Extension
  if (extension_.has_value())
  {
    const auto& ext = *extension_;
    result.push_back(static_cast<uint8_t>(ext.profile >> 8));
    result.push_back(static_cast<uint8_t>(ext.profile));
    uint16_t ext_words = (ext.data.size() + 3) / 4;
    result.push_back(static_cast<uint8_t>(ext_words >> 8));
    result.push_back(static_cast<uint8_t>(ext_words));
    result.insert(result.end(), ext.data.begin(), ext.data.end());
    // Pad to 32-bit boundary
    while (result.size() % 4 != 0)
    {
      result.push_back(0);
    }
  }

  // Payload
  result.insert(result.end(), payload_.begin(), payload_.end());

  return result;
}

void RtpPacket::set_payload(std::span<const uint8_t> data)
{
  payload_.assign(data.begin(), data.end());
}

void RtpPacket::set_payload(std::vector<uint8_t> data)
{
  payload_ = std::move(data);
}

size_t RtpPacket::size() const
{
  size_t sz = header_.header_size();
  if (extension_.has_value())
  {
    sz += 4 + ((extension_->data.size() + 3) / 4) * 4;
  }
  sz += payload_.size();
  return sz;
}

// Builder implementation
RtpPacketBuilder& RtpPacketBuilder::set_payload_type(uint8_t pt)
{
  packet_.set_payload_type(pt);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::set_sequence(uint16_t seq)
{
  packet_.set_sequence_number(seq);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::set_timestamp(uint32_t ts)
{
  packet_.set_timestamp(ts);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::set_ssrc(uint32_t ssrc)
{
  packet_.set_ssrc(ssrc);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::set_marker(bool m)
{
  packet_.set_marker(m);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::set_payload(std::span<const uint8_t> data)
{
  packet_.set_payload(data);
  return *this;
}

RtpPacketBuilder& RtpPacketBuilder::add_extension(uint16_t profile, std::span<const uint8_t> data)
{
  RtpExtension ext;
  ext.profile = profile;
  ext.data.assign(data.begin(), data.end());
  packet_.extension() = std::move(ext);
  return *this;
}

RtpPacket RtpPacketBuilder::build() const
{
  return packet_;
}

}  // namespace rtc
