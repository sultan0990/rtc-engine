#pragma once

/**
 * @file rtp_packet.h
 * @brief RTP (Real-time Transport Protocol) packet handling
 *
 * Implements RFC 3550 RTP packet parsing and building.
 */

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtc
{

/**
 * @brief RTP header structure (RFC 3550)
 *
 * Format:
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           synchronization source (SSRC) identifier            |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |            contributing source (CSRC) identifiers             |
 * |                             ....                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtpHeader
{
  uint8_t version = 2;         // RTP version (always 2)
  bool padding = false;        // Padding flag
  bool extension = false;      // Extension header present
  uint8_t csrc_count = 0;      // CSRC count
  bool marker = false;         // Marker bit
  uint8_t payload_type = 0;    // Payload type (e.g., 111 for Opus)
  uint16_t sequence = 0;       // Sequence number
  uint32_t timestamp = 0;      // Timestamp
  uint32_t ssrc = 0;           // Synchronization source
  std::vector<uint32_t> csrc;  // Contributing sources

  static constexpr size_t MIN_SIZE = 12;

  [[nodiscard]] size_t header_size() const
  {
    return MIN_SIZE + (csrc_count * 4);
  }
};

/**
 * @brief RTP header extension
 */
struct RtpExtension
{
  uint16_t profile = 0;
  std::vector<uint8_t> data;
};

/**
 * @brief Complete RTP packet
 */
class RtpPacket
{
 public:
  RtpPacket() = default;

  /**
   * @brief Parse RTP packet from raw data
   * @param data Raw packet data
   * @return Parsed packet or nullopt on error
   */
  [[nodiscard]] static std::optional<RtpPacket> parse(std::span<const uint8_t> data);

  /**
   * @brief Serialize packet to bytes
   * @return Serialized packet data
   */
  [[nodiscard]] std::vector<uint8_t> serialize() const;

  // Accessors
  [[nodiscard]] const RtpHeader& header() const
  {
    return header_;
  }
  [[nodiscard]] RtpHeader& header()
  {
    return header_;
  }

  [[nodiscard]] const std::optional<RtpExtension>& extension() const
  {
    return extension_;
  }
  [[nodiscard]] std::optional<RtpExtension>& extension()
  {
    return extension_;
  }

  [[nodiscard]] std::span<const uint8_t> payload() const
  {
    return payload_;
  }
  void set_payload(std::span<const uint8_t> data);
  void set_payload(std::vector<uint8_t> data);

  // Convenience accessors
  [[nodiscard]] uint8_t payload_type() const
  {
    return header_.payload_type;
  }
  [[nodiscard]] uint16_t sequence_number() const
  {
    return header_.sequence;
  }
  [[nodiscard]] uint32_t timestamp() const
  {
    return header_.timestamp;
  }
  [[nodiscard]] uint32_t ssrc() const
  {
    return header_.ssrc;
  }
  [[nodiscard]] bool marker() const
  {
    return header_.marker;
  }

  // Setters
  void set_payload_type(uint8_t pt)
  {
    header_.payload_type = pt;
  }
  void set_sequence_number(uint16_t seq)
  {
    header_.sequence = seq;
  }
  void set_timestamp(uint32_t ts)
  {
    header_.timestamp = ts;
  }
  void set_ssrc(uint32_t ssrc)
  {
    header_.ssrc = ssrc;
  }
  void set_marker(bool m)
  {
    header_.marker = m;
  }

  /**
   * @brief Total packet size
   */
  [[nodiscard]] size_t size() const;

 private:
  RtpHeader header_;
  std::optional<RtpExtension> extension_;
  std::vector<uint8_t> payload_;
};

/**
 * @brief RTP packet builder for constructing packets
 */
class RtpPacketBuilder
{
 public:
  RtpPacketBuilder& set_payload_type(uint8_t pt);
  RtpPacketBuilder& set_sequence(uint16_t seq);
  RtpPacketBuilder& set_timestamp(uint32_t ts);
  RtpPacketBuilder& set_ssrc(uint32_t ssrc);
  RtpPacketBuilder& set_marker(bool m);
  RtpPacketBuilder& set_payload(std::span<const uint8_t> data);
  RtpPacketBuilder& add_extension(uint16_t profile, std::span<const uint8_t> data);

  [[nodiscard]] RtpPacket build() const;

 private:
  RtpPacket packet_;
};

}  // namespace rtc
