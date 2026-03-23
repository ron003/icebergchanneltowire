#ifndef ICEBERG_CHANNEL_TO_WIRE_PCAP_READER_HPP
#define ICEBERG_CHANNEL_TO_WIRE_PCAP_READER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

/**
 * @brief Class for reading PCAP (packet capture) files
 * 
 * Implements the standard PCAP format reading for network packet capture,
 * compatible with tcpdump and Wireshark output.
 */
class PcapReader {
public:
  /**
   * @brief Open a PCAP file for reading
   * @param filename Path to the input PCAP file
   * @throws std::runtime_error if file cannot be opened or has invalid format
   */
  explicit PcapReader(const std::string& filename);

  /**
   * @brief Destructor - closes the PCAP file
   */
  ~PcapReader();

  /**
   * @brief Read the next packet from the PCAP file
   * @param packet_data Output vector to receive packet data
   * @return true if a packet was read, false if end of file
   */
  bool readPacket(std::vector<uint8_t>& packet_data);

  /**
   * @brief Check if the file was opened successfully
   * @return true if file is open and valid
   */
  bool isOpen() const { return file_.is_open(); }

  /**
   * @brief Get the maximum captured packet length from the PCAP header
   * @return snaplen value from PCAP global header
   */
  uint32_t getSnaplen() const { return snaplen_; }

  /**
   * @brief Close the PCAP file
   */
  void close();

  /**
   * @brief Reset to the beginning of packet data (after header)
   */
  void reset();

private:
  std::ifstream file_;
  std::string filename_;
  uint32_t snaplen_;
  bool header_read_;
  std::streampos data_start_pos_;

  void readHeader();
};

#endif // ICEBERG_CHANNEL_TO_WIRE_PCAP_READER_HPP
