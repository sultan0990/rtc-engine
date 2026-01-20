#pragma once

/**
 * @file rtcp_packet.h
 * @brief RTCP (RTP Control Protocol) packet handling
 *
 * Implements RFC 3550 RTCP packet types: SR, RR, SDES, BYE, APP
 * and RFC 4585 feedback messages: FIR, PLI, NACK, REMB
 */

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace rtc
{

/**
 * @brief RTCP packet types (RFC 3550)
 */
enum class RtcpType : uint8_t
{
  SR = 200,    // Sender Report
  RR = 201,    // Receiver Report
  SDES = 202,  // Source Description
  BYE = 203,   // Goodbye
  APP = 204,   // Application-specific

  // RFC 4585 - Feedback messages
  RTPFB = 205,  // Generic RTP feedback
  PSFB = 206,   // Payload-specific feedback
};

/**
 * @brief RTCP feedback message types
 */
enum class RtcpFeedbackType : uint8_t
{
  // RTPFB (205) sub-types
  NACK = 1,   // Negative acknowledgement
  TMMBR = 3,  // Temporary max media bitrate request
  TMMBN = 4,  // Temporary max media bitrate notification

  // PSFB (206) sub-types
  PLI = 1,    // Picture Loss Indication
  SLI = 2,    // Slice Loss Indication
  RPSI = 3,   // Reference Picture Selection Indication
  FIR = 4,    // Full Intra Request
  REMB = 15,  // Receiver Estimated Max Bitrate
};

/**
 * @brief RTCP common header
 */
struct RtcpHeader
{
  uint8_t version = 2;
  bool padding = false;
  uint8_t count = 0;  // Report count or format
  RtcpType type = RtcpType::SR;
  uint16_t length = 0;  // Length in 32-bit words minus one

  static constexpr size_t SIZE = 4;
};

/**
 * @brief Report block (used in SR and RR)
 */
struct RtcpReportBlock
{
  uint32_t ssrc = 0;            // SSRC of source
  uint8_t fraction_lost = 0;    // Fraction lost since last SR/RR
  uint32_t packets_lost = 0;    // Cumulative packets lost (24-bit)
  uint32_t highest_seq = 0;     // Highest sequence number received
  uint32_t jitter = 0;          // Interarrival jitter
  uint32_t last_sr = 0;         // Last SR timestamp
  uint32_t delay_since_sr = 0;  // Delay since last SR

  static constexpr size_t SIZE = 24;
};

/**
 * @brief Sender Report (SR)
 */
struct RtcpSenderReport
{
  uint32_t sender_ssrc = 0;
  uint64_t ntp_timestamp = 0;  // NTP timestamp
  uint32_t rtp_timestamp = 0;  // RTP timestamp
  uint32_t packet_count = 0;   // Sender's packet count
  uint32_t octet_count = 0;    // Sender's octet count
  std::vector<RtcpReportBlock> report_blocks;
};

/**
 * @brief Receiver Report (RR)
 */
struct RtcpReceiverReport
{
  uint32_t sender_ssrc = 0;
  std::vector<RtcpReportBlock> report_blocks;
};

/**
 * @brief Picture Loss Indication (PLI)
 */
struct RtcpPli
{
  uint32_t sender_ssrc = 0;
  uint32_t media_ssrc = 0;
};

/**
 * @brief Full Intra Request (FIR)
 */
struct RtcpFir
{
  uint32_t sender_ssrc = 0;
  uint32_t media_ssrc = 0;
  uint8_t seq_nr = 0;
};

/**
 * @brief Receiver Estimated Max Bitrate (REMB)
 */
struct RtcpRemb
{
  uint32_t sender_ssrc = 0;
  uint64_t bitrate = 0;         // Estimated max bitrate in bps
  std::vector<uint32_t> ssrcs;  // SSRCs this applies to
};

/**
 * @brief Negative Acknowledgement (NACK)
 */
struct RtcpNack
{
  uint32_t sender_ssrc = 0;
  uint32_t media_ssrc = 0;
  std::vector<uint16_t> lost_packets;  // Lost packet sequence numbers
};

/**
 * @brief Goodbye (BYE)
 */
struct RtcpBye
{
  std::vector<uint32_t> ssrcs;
  std::string reason;
};

/**
 * @brief RTCP packet (variant of all types)
 */
using RtcpPacketData = std::variant<RtcpSenderReport, RtcpReceiverReport, RtcpPli, RtcpFir,
                                    RtcpRemb, RtcpNack, RtcpBye>;

/**
 * @brief Complete RTCP packet
 */
class RtcpPacket
{
 public:
  RtcpPacket() = default;

  /**
   * @brief Parse RTCP packet from raw data
   * @param data Raw packet data
   * @return Parsed packet or nullopt on error
   */
  [[nodiscard]] static std::optional<RtcpPacket> parse(std::span<const uint8_t> data);

  /**
   * @brief Serialize packet to bytes
   * @return Serialized packet data
   */
  [[nodiscard]] std::vector<uint8_t> serialize() const;

  [[nodiscard]] const RtcpHeader& header() const
  {
    return header_;
  }
  [[nodiscard]] const RtcpPacketData& data() const
  {
    return data_;
  }

  // Factory methods for creating specific packet types
  [[nodiscard]] static RtcpPacket create_sender_report(const RtcpSenderReport& sr);
  [[nodiscard]] static RtcpPacket create_receiver_report(const RtcpReceiverReport& rr);
  [[nodiscard]] static RtcpPacket create_pli(uint32_t sender_ssrc, uint32_t media_ssrc);
  [[nodiscard]] static RtcpPacket create_fir(uint32_t sender_ssrc, uint32_t media_ssrc,
                                             uint8_t seq);
  [[nodiscard]] static RtcpPacket create_remb(uint32_t sender_ssrc, uint64_t bitrate,
                                              const std::vector<uint32_t>& ssrcs);
  [[nodiscard]] static RtcpPacket create_nack(uint32_t sender_ssrc, uint32_t media_ssrc,
                                              const std::vector<uint16_t>& lost);
  [[nodiscard]] static RtcpPacket create_bye(const std::vector<uint32_t>& ssrcs,
                                             std::string_view reason = "");

 private:
  RtcpHeader header_;
  RtcpPacketData data_;
};

}  // namespace rtc
