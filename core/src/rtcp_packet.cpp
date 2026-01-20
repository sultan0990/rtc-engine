/**
 * @file rtcp_packet.cpp
 * @brief RTCP packet implementation (stub)
 */

#include "rtc/rtcp_packet.h"

namespace rtc
{

// TODO: Implement RTCP parsing and serialization
// For now, provide stub implementations

std::optional<RtcpPacket> RtcpPacket::parse(std::span<const uint8_t> /*data*/)
{
  // TODO: Implement
  return std::nullopt;
}

std::vector<uint8_t> RtcpPacket::serialize() const
{
  // TODO: Implement
  return {};
}

RtcpPacket RtcpPacket::create_sender_report(const RtcpSenderReport& sr)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::SR;
  packet.data_ = sr;
  return packet;
}

RtcpPacket RtcpPacket::create_receiver_report(const RtcpReceiverReport& rr)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::RR;
  packet.data_ = rr;
  return packet;
}

RtcpPacket RtcpPacket::create_pli(uint32_t sender_ssrc, uint32_t media_ssrc)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::PSFB;
  packet.header_.count = static_cast<uint8_t>(RtcpFeedbackType::PLI);
  RtcpPli pli{sender_ssrc, media_ssrc};
  packet.data_ = pli;
  return packet;
}

RtcpPacket RtcpPacket::create_fir(uint32_t sender_ssrc, uint32_t media_ssrc, uint8_t seq)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::PSFB;
  packet.header_.count = static_cast<uint8_t>(RtcpFeedbackType::FIR);
  RtcpFir fir{sender_ssrc, media_ssrc, seq};
  packet.data_ = fir;
  return packet;
}

RtcpPacket RtcpPacket::create_remb(uint32_t sender_ssrc, uint64_t bitrate,
                                   const std::vector<uint32_t>& ssrcs)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::PSFB;
  packet.header_.count = static_cast<uint8_t>(RtcpFeedbackType::REMB);
  RtcpRemb remb{sender_ssrc, bitrate, ssrcs};
  packet.data_ = remb;
  return packet;
}

RtcpPacket RtcpPacket::create_nack(uint32_t sender_ssrc, uint32_t media_ssrc,
                                   const std::vector<uint16_t>& lost)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::RTPFB;
  packet.header_.count = static_cast<uint8_t>(RtcpFeedbackType::NACK);
  RtcpNack nack{sender_ssrc, media_ssrc, lost};
  packet.data_ = nack;
  return packet;
}

RtcpPacket RtcpPacket::create_bye(const std::vector<uint32_t>& ssrcs, std::string_view reason)
{
  RtcpPacket packet;
  packet.header_.type = RtcpType::BYE;
  RtcpBye bye{ssrcs, std::string(reason)};
  packet.data_ = bye;
  return packet;
}

}  // namespace rtc
