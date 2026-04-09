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

  /**
   * @brief Bulk-read packets into pre-allocated raw byte blocks.
   *
   * For each packet read from the PCAP:
   *   - Copies the network headers (Ethernet+IP+UDP) and DAQEthHeader+WIBEthHeader
   *     (total @p header_bytes_per_pkt bytes) into @p header_block.
   *   - Records the byte offset of that header in @p header_offsets.
   *   - Copies the ADC payload (@p adc_bytes_per_pkt bytes) into @p adc_block.
   *   - Records the byte offset of that ADC payload in @p adc_offsets.
   *
   * Stops when max_packets have been read or the file is exhausted.
   *
   * @param header_block       Pre-allocated buffer for header data
   * @param header_offsets     Pre-allocated buffer for header byte offsets
   * @param adc_block          Pre-allocated buffer for ADC data
   * @param adc_offsets        Pre-allocated buffer for ADC byte offsets
   * @param max_packets        Maximum number of packets to read
   * @param header_bytes_per_pkt  Bytes to copy per packet for headers
   * @param adc_bytes_per_pkt    Bytes to copy per packet for ADC payload
   * @param net_header_size      Size of network headers (Eth+IP+UDP) to skip before frame
   * @return Number of packets actually read
   */
  uint32_t readPacketsBulk(uint8_t* header_block,
                           size_t* header_offsets,
                           uint8_t* adc_block,
                           size_t* adc_offsets,
                           uint32_t max_packets,
                           size_t header_bytes_per_pkt,
                           size_t adc_bytes_per_pkt,
                           size_t net_header_size);

private:
  std::ifstream file_;
  std::string filename_;
  uint32_t snaplen_;
  bool header_read_;
  std::streampos data_start_pos_;

  void readHeader();
};

#endif // ICEBERG_CHANNEL_TO_WIRE_PCAP_READER_HPP
