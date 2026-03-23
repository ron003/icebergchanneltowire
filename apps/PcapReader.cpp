#include "PcapReader.hpp"
#include <cstring>
#include <stdexcept>

// PCAP global header (24 bytes)
struct PcapGlobalHeader {
  uint32_t magic_number;   // 0xa1b2c3d4 for standard PCAP
  uint16_t version_major;  // 2
  uint16_t version_minor;  // 4
  int32_t timezone;        // timezone offset (0 for UTC)
  uint32_t timestamp_accuracy;  // timestamp accuracy (0)
  uint32_t snaplen;        // max captured packet length
  uint32_t data_link_type; // data link type (1 = Ethernet)
};

// PCAP packet header (16 bytes)
struct PcapPacketHeader {
  uint32_t timestamp_sec;
  uint32_t timestamp_usec;
  uint32_t incl_len;  // included length (actual packet size written)
  uint32_t orig_len;  // original length (same as incl_len for captured packets)
};

PcapReader::PcapReader(const std::string& filename)
  : filename_(filename), snaplen_(0), header_read_(false), data_start_pos_(0) {
  
  file_.open(filename, std::ios::binary);
  if (!file_.is_open()) {
    throw std::runtime_error("Cannot open PCAP file for reading: " + filename);
  }
  
  readHeader();
}

PcapReader::~PcapReader() {
  close();
}

void PcapReader::readHeader() {
  PcapGlobalHeader header;
  
  file_.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (file_.fail()) {
    throw std::runtime_error("Failed to read PCAP global header from: " + filename_);
  }
  
  // Check magic number (standard PCAP format)
  if (header.magic_number != 0xa1b2c3d4) {
    // Check for byte-swapped magic (different endianness)
    if (header.magic_number == 0xd4c3b2a1) {
      throw std::runtime_error("PCAP file is in different byte order (not supported): " + filename_);
    }
    // Check for nanosecond timestamp format
    if (header.magic_number == 0xa1b23c4d || header.magic_number == 0x4d3cb2a1) {
      throw std::runtime_error("PCAP file uses nanosecond timestamps (not supported): " + filename_);
    }
    throw std::runtime_error("Invalid PCAP file (bad magic number): " + filename_);
  }
  
  // Validate version
  if (header.version_major != 2 || header.version_minor != 4) {
    throw std::runtime_error("Unsupported PCAP version " + 
                            std::to_string(header.version_major) + "." +
                            std::to_string(header.version_minor) + 
                            " in: " + filename_);
  }
  
  // Validate data link type (1 = Ethernet)
  if (header.data_link_type != 1) {
    throw std::runtime_error("Unsupported data link type " +
                            std::to_string(header.data_link_type) +
                            " (expected 1=Ethernet) in: " + filename_);
  }
  
  snaplen_ = header.snaplen;
  header_read_ = true;
  data_start_pos_ = file_.tellg();
}

bool PcapReader::readPacket(std::vector<uint8_t>& packet_data) {
  if (!file_.is_open() || !header_read_) {
    return false;
  }
  
  PcapPacketHeader pkt_header;
  
  // Read packet header
  file_.read(reinterpret_cast<char*>(&pkt_header), sizeof(pkt_header));
  if (file_.fail() || file_.eof()) {
    return false;
  }
  
  // Validate packet length
  if (pkt_header.incl_len > snaplen_ || pkt_header.incl_len == 0) {
    throw std::runtime_error("Invalid packet length in PCAP file: " + filename_);
  }
  
  // Read packet data
  packet_data.resize(pkt_header.incl_len);
  file_.read(reinterpret_cast<char*>(packet_data.data()), pkt_header.incl_len);
  if (file_.fail()) {
    return false;
  }
  
  return true;
}

void PcapReader::reset() {
  if (file_.is_open() && header_read_) {
    file_.clear();
    file_.seekg(data_start_pos_);
  }
}

void PcapReader::close() {
  if (file_.is_open()) {
    file_.close();
  }
}
