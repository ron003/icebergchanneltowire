#include "PcapWriter.hpp"
#include <cstring>
#include <chrono>
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

PcapWriter::PcapWriter(const std::string& filename, uint32_t snaplen)
  : snaplen_(snaplen), header_written_(false) {
  
  file_.open(filename, std::ios::binary);
  if (!file_.is_open()) {
    throw std::runtime_error("Cannot open PCAP file for writing: " + filename);
  }
  
  writeHeader();
}

PcapWriter::~PcapWriter() {
  close();
}

void PcapWriter::writeHeader() {
  PcapGlobalHeader header = {
    0xa1b2c3d4,  // magic number (standard PCAP)
    2,           // version major
    4,           // version minor
    0,           // timezone offset (UTC)
    0,           // timestamp accuracy
    snaplen_,    // max captured packet length
    1            // data link type (Ethernet)
  };

  file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (file_.fail()) {
    throw std::runtime_error("Failed to write PCAP global header");
  }
  
  header_written_ = true;
}

void PcapWriter::writePacket(const uint8_t* data, uint32_t length, uint64_t timestamp_us) {
  if (!file_.is_open()) {
    throw std::runtime_error("PCAP file is not open");
  }

  // Get current timestamp if not provided
  if (timestamp_us == 0) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  }

  // Create packet header
  PcapPacketHeader pkt_header = {
    static_cast<uint32_t>(timestamp_us / 1000000),  // seconds
    static_cast<uint32_t>(timestamp_us % 1000000),  // microseconds
    length,  // included length
    length   // original length
  };

  // Write packet header
  file_.write(reinterpret_cast<const char*>(&pkt_header), sizeof(pkt_header));
  if (file_.fail()) {
    throw std::runtime_error("Failed to write PCAP packet header");
  }

  // Write packet data
  file_.write(reinterpret_cast<const char*>(data), length);
  if (file_.fail()) {
    throw std::runtime_error("Failed to write PCAP packet data");
  }
}

void PcapWriter::close() {
  if (file_.is_open()) {
    file_.close();
  }
}
