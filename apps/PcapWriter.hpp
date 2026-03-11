#ifndef ICEBERG_CHANNEL_TO_WIRE_PCAP_WRITER_HPP
#define ICEBERG_CHANNEL_TO_WIRE_PCAP_WRITER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

/**
 * @brief Class for writing PCAP (packet capture) files
 * 
 * Implements the standard PCAP format for network packet capture,
 * compatible with tcpdump and Wireshark.
 */
class PcapWriter {
public:
  /**
   * @brief Create a new PCAP file for writing
   * @param filename Path to the output PCAP file
   * @param snaplen Maximum captured packet length (default 65535)
   */
  explicit PcapWriter(const std::string& filename, uint32_t snaplen = 65535);

  /**
   * @brief Destructor - closes the PCAP file
   */
  ~PcapWriter();

  /**
   * @brief Write a packet to the PCAP file
   * @param data Pointer to packet data
   * @param length Length of packet data in bytes
   * @param timestamp_us Timestamp in microseconds (default: current time)
   */
  void writePacket(const uint8_t* data, uint32_t length, uint64_t timestamp_us = 0);

  /**
   * @brief Check if the file was opened successfully
   * @return true if file is open and valid
   */
  bool isOpen() const { return file_.is_open(); }

  /**
   * @brief Close the PCAP file
   */
  void close();

private:
  std::ofstream file_;
  uint32_t snaplen_;
  bool header_written_;

  void writeHeader();
};

#endif // ICEBERG_CHANNEL_TO_WIRE_PCAP_WRITER_HPP
