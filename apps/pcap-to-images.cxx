/**
 * @file pcap-to-images.cxx
 *
 * Convert PCAP network capture file to PNG images.
 * 
 * Reads a PCAP file containing WIB Ethernet frame packets and generates
 * 6 PNG images (U0, U1, V0, V1, Z0, Z1) representing wire data for
 * ICEBERG TPC planes.
 *
 * This is the reverse operation of images-to-pcap.cxx
 *
 * Valid packet counts: 20, 40, 60, 80, or 100 (error if > 100)
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <array>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <optional>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>

#include "TRACE/trace.h"
#include "detdataformats/DAQEthHeader.hpp"
#include "fddetdataformats/WIBEthFrame.hpp"
#include "IcebergWireChannelMap.hpp"
#include "PngImageLoader.hpp"
#include "PcapReader.hpp"
// detchannelmaps factory
#include "detchannelmaps/TPCChannelMap.hpp"

namespace {

// global channel-map instance (constructed at runtime from --plugin)
std::shared_ptr<dunedaq::detchannelmaps::TPCChannelMap> global_map;

using dunedaq::fddetdataformats::WIBEthFrame;

constexpr uint16_t kChannelsPerPacket = WIBEthFrame::s_num_channels;
constexpr uint16_t kTicksPerPacket = WIBEthFrame::s_time_samples_per_frame;
constexpr uint16_t kMax14BitAdc = (1u << WIBEthFrame::s_bits_per_adc) - 1u;

// Network header sizes
constexpr size_t kEthernetHeaderSize = 14;
constexpr size_t kIPv4HeaderSize = 20;
constexpr size_t kUDPHeaderSize = 8;
constexpr size_t kTotalHeaderSize = kEthernetHeaderSize + kIPv4HeaderSize + kUDPHeaderSize;

// Valid packet counts (multiples of 20, max 100)
const std::set<uint32_t> kValidPacketCounts = {20, 40, 60, 80, 100};

// Total offline channels in ICEBERG
constexpr unsigned int kTotalChannels = 1280;

// Packets per column group (timetick window)
constexpr uint16_t kPacketsPerGroup = kTotalChannels / kChannelsPerPacket;  // 1280 / 64 = 20

/**
 * @brief Extract WIBEthFrame from a network packet
 * @param packet Raw packet data including Ethernet/IP/UDP headers
 * @param packet_size Size of packet data
 * @return Pointer to WIBEthFrame within the packet, or nullptr if invalid
 */
const WIBEthFrame* extract_wib_frame(const uint8_t* packet, size_t packet_size) {
  if (packet_size < kTotalHeaderSize + sizeof(WIBEthFrame)) {
    return nullptr;
  }
  return reinterpret_cast<const WIBEthFrame*>(packet + kTotalHeaderSize);
}

/**
 * @brief Determine the packet index (0-19) from the WIBEthFrame
 * 
 * Uses crate/slot/stream to map back to offline channel offset
 */
uint16_t get_packet_index(const WIBEthFrame& frame) {
  if (global_map) {
    // Use channel map to get the offline channel for this packet
    uint16_t crate = frame.daq_header.crate_id;
    uint16_t slot = frame.daq_header.slot_id;
    uint16_t stream = frame.daq_header.stream_id;
    uint16_t channel = frame.header.channel;
    
    // Reverse the encoding from images-to-pcap:
    // out_stream = ((coords->fiber & 0x1U) << 6) | ((coords->channel / n_chan_per_stream) & 0x3U)
    // out_chan = coords->channel % n_chan_per_stream
    constexpr unsigned int n_chan_per_stream = 64;
    unsigned int fiber = (stream >> 6) & 0x1U;
    unsigned int chan_group = stream & 0x3U;
    unsigned int map_channel = chan_group * n_chan_per_stream + channel;
    
    auto off_chan = global_map->get_offline_channel_from_crate_slot_fiber_chan(
        crate, slot, fiber, map_channel);
    
    if (off_chan) {
      return static_cast<uint16_t>(off_chan / kChannelsPerPacket);
    }
  }
  
  // Fallback: use stream_id as packet index
  return frame.daq_header.stream_id % kPacketsPerGroup;
}

} // namespace

namespace dune {
  struct DuneToolException : public std::runtime_error {
    DuneToolException(const std::string& msg) : std::runtime_error(msg) {}
  };
}

// Command line arguments structure
struct CommandLineArgs {
  std::string input_file;
  std::string output_dir = ".";
  std::string output_prefix;
  bool verbose = false;
  std::string plugin = "ICEBERGChannelMap";
  uint32_t loops = 1;  // Number of times to loop over packet data
};

// Plane information structure
struct PlaneInfo {
  char plane_char;           // 'U', 'V', or 'Z'
  unsigned int tpc_num;      // 0 or 1
  unsigned int expected_rows;
  std::vector<uint16_t> pixels;
  uint16_t width;
};

class PcapToImages {
public:
  PcapToImages() = default;

  bool parseArguments(int argc, char* argv[]);
  bool readPcap();
  bool processPackets();
  bool generateImages();

private:
  CommandLineArgs args_;
  std::map<std::string, PlaneInfo> planes_;
  uint16_t common_columns_ = 0;
  
  // Raw packet data storage (all packets in memory)
  std::vector<std::vector<uint8_t>> packets_;
  
  // Vectors of pointers into packet data:
  // 1) Pointers to DAQEthHeader (followed by WIBEthHeader)
  std::vector<const dunedaq::detdataformats::DAQEthHeader*> header_ptrs_;
  // 2) Pointers to ADC words data
  std::vector<const WIBEthFrame::word_t*> adc_ptrs_;
  
  // Pre-computed offline channel mappings for each plane
  // Maps plane key -> vector of offline channel IDs (one per wire)
  std::map<std::string, std::vector<raw::ChannelID_t>> offline_channels_;
  
  uint32_t getPlaneIndex(char plane_char) const;
  raw::ChannelID_t offlineChannelForWire(char plane_char,
                                         unsigned int tpc_num,
                                         unsigned int wire_num) const;
  std::string buildOutputFilename(char plane_char, unsigned int tpc_num) const;
  bool saveImage(const std::string& filename, const PlaneInfo& plane);
  void initializePlanePixelData();
  void populatePlanePixelData(uint32_t pkt_idx, const WIBEthFrame* frame);

  geo::IcebergWireChannelMap channel_map_;
};

uint32_t PcapToImages::getPlaneIndex(char plane_char) const {
  if (plane_char == 'U') return 0;
  if (plane_char == 'V') return 1;
  if (plane_char == 'Z') return 2;
  throw std::runtime_error(std::string("Invalid plane character: ") + plane_char);
}

raw::ChannelID_t PcapToImages::offlineChannelForWire(char plane_char,
                                                     unsigned int tpc_num,
                                                     unsigned int wire_num) const {
  geo::WireID wire_id(0, tpc_num, getPlaneIndex(plane_char), wire_num);
  return channel_map_.PlaneWireToChannel(wire_id);
}

std::string PcapToImages::buildOutputFilename(char plane_char, 
                                               unsigned int tpc_num) const {
  std::string filename = args_.output_prefix + std::string(1, plane_char) +
                         std::to_string(tpc_num) + "x" +
                         std::to_string(common_columns_) + ".png";
  return args_.output_dir + "/" + filename;
}

bool PcapToImages::saveImage(const std::string& filename, const PlaneInfo& plane) {
  try {
    PngImageLoader::saveImage(filename, plane.width, plane.expected_rows, plane.pixels);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to save image " << filename
              << ": " << e.what() << std::endl;
    return false;
  }
}

void PcapToImages::initializePlanePixelData() {
  // Build offline channel mappings and allocate pixel data for each plane
  for (auto& [key, plane] : planes_) {
    // Build channel mapping
    auto& channels = offline_channels_[key];
    channels.reserve(plane.expected_rows);
    for (unsigned int wire = 0; wire < plane.expected_rows; ++wire) {
      channels.push_back(offlineChannelForWire(plane.plane_char, plane.tpc_num, wire));
    }
    
    if (args_.verbose && !channels.empty()) {
      std::cout << "Mapped " << key << " wires to offline channels "
                << channels.front() << "..." << channels.back() << std::endl;
    }
    
    // Allocate pixel data (row-major: height x width)
    plane.width = common_columns_;
    plane.pixels.resize(static_cast<size_t>(plane.expected_rows) * common_columns_, 0);
  }
}

void PcapToImages::populatePlanePixelData(uint32_t pkt_idx, const WIBEthFrame* frame) {
  // Determine packet position
  uint16_t col_group = static_cast<uint16_t>(pkt_idx / kPacketsPerGroup);
  uint16_t packet_index = static_cast<uint16_t>(pkt_idx % kPacketsPerGroup);
  uint16_t tick_offset = col_group * kTicksPerPacket;
  uint16_t channel_offset = packet_index * kChannelsPerPacket;
  
  // For each plane, check if any of its channels fall within this packet's range
  for (auto& [key, plane] : planes_) {
    const auto& channels = offline_channels_.at(key);
    
    for (unsigned int wire = 0; wire < plane.expected_rows; ++wire) {
      if (wire >= channels.size()) continue;
      raw::ChannelID_t offline_ch = channels[wire];
      
      // Check if this offline channel is in the current packet's range
      if (offline_ch >= channel_offset && 
          offline_ch < (unsigned int)(channel_offset + kChannelsPerPacket)) {
        uint16_t frame_channel = static_cast<uint16_t>(offline_ch - channel_offset);
        
        // Extract ADC values for all time samples in this packet
        for (uint16_t sample = 0; sample < kTicksPerPacket; ++sample) {
          uint16_t tick = tick_offset + sample;
          if (tick < common_columns_) {
            uint16_t adc = frame->get_adc(frame_channel, sample);
            plane.pixels[static_cast<size_t>(wire) * common_columns_ + tick] = adc;
          }
        }
      }
    }
  }
}

bool PcapToImages::parseArguments(int argc, char* argv[]) {
  // Initialize plane information
  planes_["u0"] = {'U', 0, 316, {}, 0};
  planes_["u1"] = {'U', 1, 316, {}, 0};
  planes_["v0"] = {'V', 0, 315, {}, 0};
  planes_["v1"] = {'V', 1, 315, {}, 0};
  planes_["z0"] = {'Z', 0, 240, {}, 0};
  planes_["z1"] = {'Z', 1, 240, {}, 0};

  // Parse command line
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] INPUT_PCAP" << std::endl
                << std::endl
                << "Convert PCAP file to 6 PNG images (U0, U1, V0, V1, Z0, Z1)" << std::endl
                << std::endl
                << "Options:" << std::endl
                << "  --output-dir DIR   Directory for output images (default: '.')" << std::endl
                << "  --output-prefix STR Prefix prepended to output filenames" << std::endl
                << "  --plugin NAME      Channel map plugin (default: 'ICEBERGChannelMap')" << std::endl
                << "  --loops N          Number of times to loop over packet data (default: 1)" << std::endl
                << "  --verbose          Enable verbose output" << std::endl
                << "  -h, --help         Show this help message" << std::endl
                << std::endl
                << "Valid packet counts: 20, 40, 60, 80, or 100" << std::endl
                << "  20 packets  -> 64 columns" << std::endl
                << "  40 packets  -> 128 columns" << std::endl
                << "  60 packets  -> 192 columns" << std::endl
                << "  80 packets  -> 256 columns" << std::endl
                << "  100 packets -> 320 columns" << std::endl;
      return false;
    }
    else if (arg == "--output-dir" && i + 1 < argc) {
      args_.output_dir = argv[++i];
    }
    else if (arg == "--output-prefix" && i + 1 < argc) {
      args_.output_prefix = argv[++i];
    }
    else if (arg == "--plugin" && i + 1 < argc) {
      args_.plugin = argv[++i];
    }
    else if (arg == "--loops" && i + 1 < argc) {
      args_.loops = static_cast<uint32_t>(std::atoi(argv[++i]));
      if (args_.loops == 0) {
        std::cerr << "ERROR: --loops must be at least 1" << std::endl;
        return false;
      }
    }
    else if (arg == "--verbose") {
      args_.verbose = true;
    }
    else if (arg[0] != '-') {
      // Positional argument: input PCAP file
      if (args_.input_file.empty()) {
        args_.input_file = arg;
      } else {
        std::cerr << "ERROR: Multiple input files specified" << std::endl;
        return false;
      }
    }
    else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return false;
    }
  }

  if (args_.input_file.empty()) {
    std::cerr << "ERROR: No input PCAP file specified" << std::endl;
    return false;
  }

  return true;
}

bool PcapToImages::readPcap() {
  std::cout << "Reading PCAP file: " << args_.input_file << std::endl;

  try {
    // Construct the detchannelmaps instance from the selected plugin
    try {
      global_map = dunedaq::detchannelmaps::make_map(args_.plugin);
    } catch (const std::exception& e) {
      std::cerr << "WARNING: Failed to load channel map plugin '" << args_.plugin
                << "': " << e.what() << std::endl;
      std::cerr << "Using fallback packet ordering" << std::endl;
      global_map.reset();
    }

    PcapReader pcap(args_.input_file);
    
    // Read all packets into memory
    std::vector<uint8_t> packet_data;
    while (pcap.readPacket(packet_data)) {
      packets_.push_back(std::move(packet_data));
      packet_data.clear();
    }
    
    uint32_t packet_count = static_cast<uint32_t>(packets_.size());
    std::cout << "Read " << packet_count << " packets from PCAP" << std::endl;
    
    // Validate packet count
    if (packet_count > 100) {
      std::cerr << "ERROR: PCAP file contains " << packet_count 
                << " packets. Maximum allowed is 100." << std::endl;
      return false;
    }
    
    if (kValidPacketCounts.find(packet_count) == kValidPacketCounts.end()) {
      std::cerr << "ERROR: Invalid packet count " << packet_count 
                << ". Must be 20, 40, 60, 80, or 100." << std::endl;
      return false;
    }
    
    // Calculate dimensions from packet count
    uint16_t num_column_groups = static_cast<uint16_t>(packet_count / kPacketsPerGroup);
    common_columns_ = num_column_groups * kTicksPerPacket;
    
    std::cout << "Detected " << num_column_groups << " column group(s), "
              << common_columns_ << " total columns" << std::endl;
    
    // Build pointer vectors for direct access to headers and ADC data
    header_ptrs_.reserve(packets_.size());
    adc_ptrs_.reserve(packets_.size());
    
    for (uint32_t pkt_idx = 0; pkt_idx < packets_.size(); ++pkt_idx) {
      const auto& pkt = packets_[pkt_idx];
      
      if (pkt.size() < kTotalHeaderSize + sizeof(WIBEthFrame)) {
        std::cerr << "WARNING: Packet " << pkt_idx << " is too small to contain WIBEthFrame" << std::endl;
        header_ptrs_.push_back(nullptr);
        adc_ptrs_.push_back(nullptr);
        continue;
      }
      
      // Pointer to DAQEthHeader (which is followed by WIBEthHeader within WIBEthFrame)
      const auto* frame = reinterpret_cast<const WIBEthFrame*>(pkt.data() + kTotalHeaderSize);
      header_ptrs_.push_back(&frame->daq_header);
      
      // Pointer to ADC words data
      adc_ptrs_.push_back(&frame->adc_words[0][0]);
      
      if (args_.verbose) {
        std::cout << "Packet " << pkt_idx 
                  << ": header_ptr=" << static_cast<const void*>(header_ptrs_.back())
                  << ", adc_ptr=" << static_cast<const void*>(adc_ptrs_.back()) << std::endl;
      }
    }
    
    std::cout << "Created " << header_ptrs_.size() << " header pointers and "
              << adc_ptrs_.size() << " ADC pointers" << std::endl;
    
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to read PCAP: " << e.what() << std::endl;
    return false;
  }
}

bool PcapToImages::processPackets() {
  std::cout << "Processing packets with " << args_.loops << " loop(s)..." << std::endl;
  
  // Initialize plane pixel data and channel mappings
  initializePlanePixelData();
  
  for (uint32_t loop = 0; loop < args_.loops; ++loop) {
    if (args_.verbose || args_.loops > 1) {
      std::cout << "Loop " << (loop + 1) << " of " << args_.loops << std::endl;
    }
    
    // Process each packet using the pointer vectors
    for (uint32_t pkt_idx = 0; pkt_idx < header_ptrs_.size(); ++pkt_idx) {
      const auto* header = header_ptrs_[pkt_idx];
      const auto* adc_data = adc_ptrs_[pkt_idx];
      
      if (!header || !adc_data) {
        continue;
      }
      
      // Reconstruct the WIBEthFrame pointer from header pointer
      // (header is at the start of WIBEthFrame)
      const auto* frame = reinterpret_cast<const WIBEthFrame*>(header);
      
      if (args_.verbose) {
        uint16_t col_group = static_cast<uint16_t>(pkt_idx / kPacketsPerGroup);
        uint16_t packet_index = static_cast<uint16_t>(pkt_idx % kPacketsPerGroup);
        uint16_t tick_offset = col_group * kTicksPerPacket;
        uint16_t channel_offset = packet_index * kChannelsPerPacket;
        
        std::cout << "Processing packet " << pkt_idx 
                  << ": col_group=" << col_group
                  << ", packet_index=" << packet_index
                  << ", channels " << channel_offset << "-" 
                  << (channel_offset + kChannelsPerPacket - 1)
                  << ", ticks " << tick_offset << "-"
                  << (tick_offset + kTicksPerPacket - 1) << std::endl;
      }
      
      // Extract ADC values and populate all 6 plane pixel data blocks
      populatePlanePixelData(pkt_idx, frame);
    }
  }
  
  TLOG_DEBUG(1) << "All 6 plane pixel data blocks populated with "
                << common_columns_ << " columns";
  
  return true;
}

bool PcapToImages::generateImages() {
  std::cout << "Writing " << planes_.size() << " PNG images..." << std::endl;

  try {
    // Create output directory if it doesn't exist
    if (!args_.output_dir.empty() && args_.output_dir != ".") {
      std::filesystem::create_directories(args_.output_dir);
    }
    
    // Write each plane's already-populated pixel data to a PNG file
    for (const auto& [key, plane] : planes_) {
      std::string filename = buildOutputFilename(plane.plane_char, plane.tpc_num);
      std::cout << "Writing " << key << ": " << plane.width << "x" 
                << plane.expected_rows << " -> " << filename << std::endl;
      
      if (!saveImage(filename, plane)) {
        return false;
      }
    }
    
    std::cout << "All images written successfully" << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to write images: " << e.what() << std::endl;
    return false;
  }
}

int main(int argc, char* argv[]) {
  try {
    PcapToImages converter;

    if (!converter.parseArguments(argc, argv)) {
      return 1;
    }

    if (!converter.readPcap()) {
      std::cerr << "Failed to read PCAP file" << std::endl;
      return 1;
    }

    if (!converter.processPackets()) {
      std::cerr << "Failed to process packets" << std::endl;
      return 1;
    }

    if (!converter.generateImages()) {
      std::cerr << "Failed to generate images" << std::endl;
      return 1;
    }

    std::cout << "Success!" << std::endl;
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
