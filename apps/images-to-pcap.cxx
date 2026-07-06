/**
 * @file images-to-pcap.cxx
 *
 * Convert PNG images to PCAP network capture file.
 * 
 * Reads 6 PNG images (U0, U1, V0, V1, Z0, Z1) representing wire data for
 * ICEBERG TPC planes and generates simulated UDP packets that are written
 * to a PCAP file.
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
#include <limits>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "TRACE/trace.h"
#include "fddetdataformats/WIBEthFrame.hpp"
#include "IcebergWireChannelMap.hpp"
#include "PngImageLoader.hpp"
#include "PcapWriter.hpp"
// detchannelmaps factory
#include "detchannelmaps/TPCChannelMap.hpp"
#include "PacketOrderMap.hpp"

// raw::ChannelID_t type definition
namespace raw {
  using ChannelID_t = unsigned int;
}

namespace {

// global channel-map instance (constructed at runtime from --plugin)
std::shared_ptr<dunedaq::detchannelmaps::TPCChannelMap> global_map;

using dunedaq::fddetdataformats::WIBEthFrame;

constexpr uint16_t kChannelsPerPacket = WIBEthFrame::s_num_channels;
constexpr uint16_t kTicksPerPacket = WIBEthFrame::s_time_samples_per_frame;
constexpr uint16_t kMax14BitAdc = (1u << WIBEthFrame::s_bits_per_adc) - 1u;

struct EthernetHeader {
  std::array<uint8_t, 6> dst_mac;
  std::array<uint8_t, 6> src_mac;
  uint16_t ether_type;
} __attribute__((packed));

struct IPv4Header {
  uint8_t version_ihl;
  uint8_t dscp_ecn;
  uint16_t total_length;
  uint16_t identification;
  uint16_t flags_fragment_offset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t src_ip;
  uint32_t dst_ip;
} __attribute__((packed));

struct UDPHeader {
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t length;
  uint16_t checksum;
} __attribute__((packed));

static_assert(sizeof(EthernetHeader) == 14, "Unexpected Ethernet header size");
static_assert(sizeof(IPv4Header) == 20, "Unexpected IPv4 header size");
static_assert(sizeof(UDPHeader) == 8, "Unexpected UDP header size");

uint16_t
compute_ipv4_checksum(const uint8_t* data, size_t length)
{
  uint32_t sum = 0;
  for (size_t index = 0; index + 1 < length; index += 2) {
    sum += (static_cast<uint32_t>(data[index]) << 8) | data[index + 1];
  }
  if ((length & 1U) != 0U) {
    sum += static_cast<uint32_t>(data[length - 1]) << 8;
  }
  while ((sum >> 16) != 0U) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return static_cast<uint16_t>(~sum & 0xFFFFU);
}

uint16_t
clamp_to_14bit(uint16_t value)
{
  return std::min<uint16_t>(value, kMax14BitAdc);
}

WIBEthFrame
build_wib_frame(const std::vector<std::vector<uint16_t>>& pixel_data_block,
                uint16_t channel_offset,
                uint16_t tick_offset,
                uint16_t packet_index,
                uint64_t timestamp,
                uint16_t sequence_id)
{
  WIBEthFrame frame {};
  frame.daq_header.version = 1;
  frame.daq_header.det_id = 3;
  // Default values in case global_map is not set
  frame.daq_header.crate_id = 0;
  frame.daq_header.slot_id = 0;
  frame.daq_header.stream_id = packet_index;
  TLOG_DEBUG(2) << "Setting DAQ header: det_id=" << frame.daq_header.det_id
                << ", crate_id=" << frame.daq_header.crate_id
                << ", slot_id=" << frame.daq_header.slot_id
                << ", stream_id=" << frame.daq_header.stream_id;
  frame.daq_header.reserved = 0;
  frame.daq_header.seq_id = sequence_id & 0x0FFFu;
  frame.daq_header.block_length = sizeof(WIBEthFrame) / sizeof(WIBEthFrame::word_t);
  frame.set_timestamp(timestamp);

  // If a channel map is available, use the coordinates for the first offline
  // channel in this packet to populate crate/slot/stream and the packet channel.
  if (global_map) {
    TLOG_DEBUG(1) << "Building WIB frame for packet " << packet_index
                  << " with channel offset " << channel_offset
                  << " and tick offset " << tick_offset;
    uint32_t off_chan0 = static_cast<uint32_t>(channel_offset);
    auto coords = global_map->get_crate_slot_fiber_chan_from_offline_channel(off_chan0);
    constexpr unsigned int n_chan_per_stream = 64;
    const unsigned int out_stream = ((coords->fiber & 0x1U) << 6) | ((coords->channel / n_chan_per_stream) & 0x3U);
    const unsigned int out_chan = coords->channel % n_chan_per_stream;

    frame.daq_header.crate_id = static_cast<uint16_t>(coords->crate);
    frame.daq_header.slot_id = static_cast<uint16_t>(coords->slot);
    frame.daq_header.stream_id = static_cast<uint16_t>(out_stream);
    frame.header.channel = static_cast<uint16_t>(out_chan);
  } else {
    frame.header.channel = packet_index;
  }
  frame.header.version = 1;
  frame.header.context = packet_index & 0xFFu;
  frame.header.ready = 1;
  frame.header.link_valid = 0x3u;
  frame.header.wib_sync = 1;
  frame.header.femb_sync = 0x3u;
  frame.header.colddata_timestamp_0 = timestamp & 0x7FFFu;
  frame.header.colddata_timestamp_1 = (timestamp >> 15) & 0x7FFFu;
  frame.header.extra_data = 0;

  for (uint16_t sample = 0; sample < kTicksPerPacket; ++sample) {
    for (uint16_t channel = 0; channel < kChannelsPerPacket; ++channel) {
      uint16_t const adc = clamp_to_14bit(pixel_data_block[channel_offset + channel][tick_offset + sample]);
      frame.set_adc(channel, sample, adc);
    }
  }

  return frame;
}

std::vector<uint8_t>
build_network_packet(const WIBEthFrame& frame, uint16_t packet_id)
{
  size_t constexpr ethernet_header_size = sizeof(EthernetHeader);
  size_t constexpr ip_header_size = sizeof(IPv4Header);
  size_t constexpr udp_header_size = sizeof(UDPHeader);
  size_t constexpr payload_size = sizeof(WIBEthFrame);
  size_t constexpr packet_size = ethernet_header_size + ip_header_size + udp_header_size + payload_size;

  std::vector<uint8_t> packet(packet_size, 0);

  auto* ethernet = reinterpret_cast<EthernetHeader*>(packet.data());
  ethernet->dst_mac = { 0x02, 0x00, 0x00, 0x00, 0x10, 0x01 };
  ethernet->src_mac = { 0x02, 0x00, 0x00, 0x00, 0x20, static_cast<uint8_t>(packet_id & 0xFFu) };
  ethernet->ether_type = htons(0x0800);

  auto* ip = reinterpret_cast<IPv4Header*>(packet.data() + ethernet_header_size);
  ip->version_ihl = 0x45;
  ip->dscp_ecn = 0;
  ip->total_length = htons(ip_header_size + udp_header_size + payload_size);
  ip->identification = htons(packet_id);
  ip->flags_fragment_offset = htons(0x4000);
  ip->ttl = 64;
  ip->protocol = 17;
  ip->checksum = 0;
  ip->src_ip = htonl(0x0A000001u + packet_id);
  ip->dst_ip = htonl(0x0A000101u);
  ip->checksum = htons(compute_ipv4_checksum(reinterpret_cast<const uint8_t*>(ip), ip_header_size));

  auto* udp = reinterpret_cast<UDPHeader*>(packet.data() + ethernet_header_size + ip_header_size);
  udp->src_port = htons(static_cast<uint16_t>(40000u + (packet_id % 1000u)));
  udp->dst_port = htons(50000u);
  udp->length = htons(udp_header_size + payload_size);
  udp->checksum = 0;

  std::memcpy(packet.data() + ethernet_header_size + ip_header_size + udp_header_size,
              &frame,
              sizeof(frame));

  return packet;
}

} // namespace

namespace dune {
  struct DuneToolException : public std::runtime_error {
    DuneToolException(const std::string& msg) : std::runtime_error(msg) {}
  };
}

// Command line arguments structure
struct CommandLineArgs {
  std::string u0_image;
  std::string u1_image;
  std::string v0_image;
  std::string v1_image;
  std::string z0_image;
  std::string z1_image;
  std::string image_dir = ".";
  std::string image_prefix;
  std::string output_file = "output.pcap";
  bool verbose = false;
  uint16_t timestamp_us = 0;
  std::string plugin = "ICEBERGChannelMap";
};

// Image plane information
struct PlaneInfo {
  char plane_char;           // 'U', 'V', or 'Z'
  unsigned int tpc_num;      // 0 or 1
  unsigned int expected_rows;
  PngImageLoader::ImageData data;
  bool provided;
};

class ImagesTopcap {
public:
  ImagesTopcap() = default;

  bool parseArguments(int argc, char* argv[]);
  bool processImages();
  bool generatePcap();

private:
  CommandLineArgs args_;
  std::map<std::string, PlaneInfo> planes_;
  uint16_t common_columns_;

  uint32_t getPlaneIndex(char plane_char) const;
  bool validateImage(const PlaneInfo& plane);
  bool loadOrGenerateImage(PlaneInfo& plane,
                          const std::string& plane_name);
  std::string buildImageFilename(char plane_char, unsigned int tpc_num,
                                 uint16_t cols) const;

  // Wire-to-offline-channel mapping built from ICEBERGChannelMap
  // Key: plane*10 + tpc, Value: vector of offline channels (index = wire number)
  std::map<int, std::vector<unsigned int>> wire_to_offline_;
  unsigned int total_offline_channels_ = 0;

  // Packet order derived from the channel map plugin
  PacketOrderMap packet_order_;

  bool buildWireMapping();
  std::optional<raw::ChannelID_t> offlineChannelForWire(char plane_char,
                                                        unsigned int tpc_num,
                                                        unsigned int wire_num) const;
};

uint32_t ImagesTopcap::getPlaneIndex(char plane_char) const {
  if (plane_char == 'U') return 0;
  if (plane_char == 'V') return 1;
  if (plane_char == 'Z') return 2;
  throw std::runtime_error(std::string("Invalid plane character: ") + plane_char);
}

bool ImagesTopcap::validateImage(const PlaneInfo& plane) {
  // Check dimensions
  if (plane.data.height != plane.expected_rows) {
    std::cerr << "ERROR: Plane " << plane.plane_char << plane.tpc_num
              << " has wrong height: " << plane.data.height
              << " (expected " << plane.expected_rows << ")" << std::endl;
    return false;
  }

  // Check that columns is a positive multiple of 64 and no more than 512.
  if (plane.data.width % 64 != 0 || plane.data.width > 512 || plane.data.width == 0) {
    std::cerr << "ERROR: Plane " << plane.plane_char << plane.tpc_num
              << " has invalid width: " << plane.data.width
              << " (must be a positive multiple of 64, max 512)" << std::endl;
    return false;
  }

  return true;
}

std::string ImagesTopcap::buildImageFilename(char plane_char, 
                                             unsigned int tpc_num,
                                             uint16_t cols) const {
  std::string filename = args_.image_prefix + std::string(1, plane_char) +
                         std::to_string(tpc_num) + "x" +
                         std::to_string(cols) + ".png";
  return args_.image_dir + "/" + filename;
}

bool ImagesTopcap::loadOrGenerateImage(PlaneInfo& plane,
                                       const std::string& plane_name) {
  if (plane.provided) {
    // Try to load the provided image
    try {
      plane.data = PngImageLoader::loadImage(plane_name);
      if (!validateImage(plane)) {
        return false;
      }
      std::cout << "Loaded " << plane_name << std::endl;
      return true;
    } catch (const std::exception& e) {
      std::cerr << "ERROR: Failed to load provided image " << plane_name
                << ": " << e.what() << std::endl;
      return false;
    }
  } else {
    // Try to find or generate the image
    // First, try to find a file with matching columns
    if (common_columns_ > 0) {
      std::string filename = buildImageFilename(plane.plane_char, 
                                               plane.tpc_num, 
                                               common_columns_);
      try {
        plane.data = PngImageLoader::loadImage(filename);
        if (validateImage(plane)) {
          std::cout << "Found " << filename << std::endl;
          return true;
        }
      } catch (...) {
        // File not found, continue to generation
      }
    }

    // Generate a test image if common_columns_ is set
    if (common_columns_ > 0) {
      std::string filename = buildImageFilename(plane.plane_char,
                                               plane.tpc_num,
                                               common_columns_);
      try {
        std::cout << "Generating test image: " << filename << std::endl;
        
        // Use IcebergWireChannelMap to create a channel-based pixel function.
        // This ensures wrapped wires (which share offline channels) get the same
        // pixel value in both TPC images, making round-trip tests deterministic.
        geo::IcebergWireChannelMap geom_map;
        unsigned int plane_idx = getPlaneIndex(plane.plane_char);
        unsigned int tpc_num = plane.tpc_num;
        
        auto pixel_func = [&geom_map, plane_idx, tpc_num](uint16_t row, uint16_t col) -> uint16_t {
          // row = wire number, col = timetick
          // Use offline channel number in the low byte instead of wire number
          // This makes wrapped wires (which share channels) have identical values
          geo::WireID wire_id(0, tpc_num, plane_idx, row); // cryostat=0
          raw::ChannelID_t ch = geom_map.PlaneWireToChannel(wire_id);
          return static_cast<uint16_t>(((col & 0x3f) << 8) | (ch & 0xff));
        };
        
        PngImageLoader::generateTestImage(filename, common_columns_, 
                                         plane.expected_rows, pixel_func);
        plane.data = PngImageLoader::loadImage(filename);
        if (validateImage(plane)) {
          return true;
        }
      } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to generate image " << filename
                  << ": " << e.what() << std::endl;
        return false;
      }
    }

    std::cerr << "ERROR: Could not load or generate image for plane "
              << plane.plane_char << plane.tpc_num << std::endl;
    return false;
  }
}

// Build a mapping from (plane, tpc, wire) -> offline_channel using IcebergWireChannelMap.
// This ensures we use the same geometry wire numbering as pcap-to-images.
bool ImagesTopcap::buildWireMapping() {
  if (!global_map) {
    std::cerr << "ERROR: Channel map not initialized" << std::endl;
    return false;
  }

  // Use IcebergWireChannelMap for consistent wire numbering with pcap-to-images
  geo::IcebergWireChannelMap geom_map;

  // Pre-size wire_to_offline_ vectors based on expected wire counts
  // U planes: 316 wires, V planes: 315 wires, Z planes: 240 wires
  constexpr std::array<unsigned int, 3> kWiresPerPlane = {316, 315, 240};
  for (unsigned int plane = 0; plane < 3; ++plane) {
    for (unsigned int tpc = 0; tpc < 2; ++tpc) {
      int key = static_cast<int>(plane) * 10 + static_cast<int>(tpc);
      // Initialize with invalid channel marker (max value)
      wire_to_offline_[key].assign(kWiresPerPlane[plane], std::numeric_limits<unsigned int>::max());
    }
  }

  // Iterate over all offline channels and use ChannelToWire to get geometry wire
  constexpr unsigned int max_offline = 1280;
  unsigned int mapped_channel_count = 0;  // Count of unique offline channels processed
  for (unsigned int off_ch = 0; off_ch < max_offline; ++off_ch) {
    // Use IcebergWireChannelMap to get the geometry wire
    std::vector<geo::WireID> wire_ids = geom_map.ChannelToWire(off_ch);
    if (wire_ids.empty()) {
      continue;
    }

    // Also verify the channel map returns consistent data
    auto coords = global_map->get_crate_slot_fiber_chan_from_offline_channel(off_ch);
    if (!coords.has_value()) {
      continue;
    }

    // This offline channel is valid - count it once
    ++mapped_channel_count;

    // Process ALL wire IDs - for wrapped wires, a single offline channel
    // maps to multiple geometry wires (e.g., TPC 0 wire 200 AND TPC 1 wire 0)
    for (const auto& wire_id : wire_ids) {
      unsigned int plane = wire_id.Plane;
      unsigned int tpc = wire_id.TPC;
      unsigned int wire = wire_id.Wire;

      if (plane > 2 || tpc > 1) {
        TLOG_DEBUG(2) << "Skipping offline channel " << off_ch
                      << " with invalid plane=" << plane << " or tpc=" << tpc;
        continue;
      }

      int key = static_cast<int>(plane) * 10 + static_cast<int>(tpc);
      if (wire < wire_to_offline_[key].size()) {
        wire_to_offline_[key][wire] = off_ch;
        TLOG_DEBUG(4) << "Mapped offline channel " << off_ch 
                      << " to plane=" << plane << " tpc=" << tpc << " wire=" << wire;
      }
    }
  }

  total_offline_channels_ = mapped_channel_count;
  TLOG_DEBUG(1) << "Mapped " << total_offline_channels_ << " offline channels to geometry wires";

  // Log summary for each plane
  for (unsigned int plane = 0; plane < 3; ++plane) {
    for (unsigned int tpc = 0; tpc < 2; ++tpc) {
      int key = static_cast<int>(plane) * 10 + static_cast<int>(tpc);
      const auto& wire_vec = wire_to_offline_[key];
      unsigned int valid_count = 0;
      for (auto ch : wire_vec) {
        if (ch != std::numeric_limits<unsigned int>::max()) {
          ++valid_count;
        }
      }
      char plane_char = (plane == 0) ? 'U' : (plane == 1) ? 'V' : 'Z';
      TLOG_DEBUG(1) << "Plane " << plane_char << tpc << " has " << valid_count
                    << " of " << wire_vec.size() << " wires mapped";
    }
  }

  return true;
}

std::optional<raw::ChannelID_t> ImagesTopcap::offlineChannelForWire(char plane_char,
                                                                    unsigned int tpc_num,
                                                                    unsigned int wire_num) const {
  unsigned int plane = getPlaneIndex(plane_char);
  int key = static_cast<int>(plane) * 10 + static_cast<int>(tpc_num);

  auto it = wire_to_offline_.find(key);
  if (it == wire_to_offline_.end()) {
    return std::nullopt;
  }

  const auto& wire_vec = it->second;
  if (wire_num >= wire_vec.size()) {
    return std::nullopt;
  }

  return wire_vec[wire_num];
}

bool ImagesTopcap::parseArguments(int argc, char* argv[]) {
  // Initialize plane information
  planes_["u0"] = {'U', 0, 316, {}, false};
  planes_["u1"] = {'U', 1, 316, {}, false};
  planes_["v0"] = {'V', 0, 315, {}, false};
  planes_["v1"] = {'V', 1, 315, {}, false};
  planes_["z0"] = {'Z', 0, 240, {}, false};
  planes_["z1"] = {'Z', 1, 240, {}, false};

  common_columns_ = 0;

  // Parse command line
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl
                << "Options:" << std::endl
                << "  --u0 FILE          Path to U plane 0 PNG image" << std::endl
                << "  --u1 FILE          Path to U plane 1 PNG image" << std::endl
                << "  --v0 FILE          Path to V plane 0 PNG image" << std::endl
                << "  --v1 FILE          Path to V plane 1 PNG image" << std::endl
                << "  --z0 FILE          Path to Z plane 0 PNG image" << std::endl
                << "  --z1 FILE          Path to Z plane 1 PNG image" << std::endl
                << "  --image-dir DIR    Directory for auto-generated images (default: '.')" << std::endl
                << "  --image-prefix STR Prefix prepended to auto image filenames" << std::endl
                << "  --output FILE      Output PCAP file (default: 'output.pcap')" << std::endl
                << "  --columns N        Force column count (must be a positive multiple of 64, max 512)" << std::endl
                << "  --verbose          Enable verbose output" << std::endl
                << "  -h, --help         Show this help message" << std::endl;
      return false;
    }
    else if (arg == "--u0" && i + 1 < argc) {
      planes_["u0"].provided = true;
      args_.u0_image = argv[++i];
    }
    else if (arg == "--u1" && i + 1 < argc) {
      planes_["u1"].provided = true;
      args_.u1_image = argv[++i];
    }
    else if (arg == "--v0" && i + 1 < argc) {
      planes_["v0"].provided = true;
      args_.v0_image = argv[++i];
    }
    else if (arg == "--v1" && i + 1 < argc) {
      planes_["v1"].provided = true;
      args_.v1_image = argv[++i];
    }
    else if (arg == "--z0" && i + 1 < argc) {
      planes_["z0"].provided = true;
      args_.z0_image = argv[++i];
    }
    else if (arg == "--z1" && i + 1 < argc) {
      planes_["z1"].provided = true;
      args_.z1_image = argv[++i];
    }
    else if (arg == "--image-dir" && i + 1 < argc) {
      args_.image_dir = argv[++i];
    }
    else if (arg == "--image-prefix" && i + 1 < argc) {
      args_.image_prefix = argv[++i];
    }
    else if (arg == "--output" && i + 1 < argc) {
      args_.output_file = argv[++i];
    }
    else if (arg == "--columns" && i + 1 < argc) {
      common_columns_ = std::atoi(argv[++i]);
      if (common_columns_ % 64 != 0 || common_columns_ > 512 || common_columns_ == 0) {
        std::cerr << "ERROR: --columns must be a positive multiple of 64 and at most 512" << std::endl;
        return false;
      }
    }
    else if (arg == "--verbose") {
      args_.verbose = true;
    }
    else if (arg == "--plugin" && i + 1 < argc) {
      args_.plugin = argv[++i];
    }
    else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return false;
    }
  }

  return true;
}

bool ImagesTopcap::processImages() {
  std::cout << "Processing images..." << std::endl;

  // Load provided images first to determine common column count
  uint16_t loaded_columns = 0;
  bool first_loaded = false;

  for (auto& [key, plane] : planes_) {
    std::string image_path;
    if (plane.provided) {
      // Get the image path from command line args
      if (key == "u0") image_path = args_.u0_image;
      else if (key == "u1") image_path = args_.u1_image;
      else if (key == "v0") image_path = args_.v0_image;
      else if (key == "v1") image_path = args_.v1_image;
      else if (key == "z0") image_path = args_.z0_image;
      else if (key == "z1") image_path = args_.z1_image;

      try {
        plane.data = PngImageLoader::loadImage(image_path);
        if (!validateImage(plane)) {
          return false;
        }

        // Check column consistency
        if (!first_loaded) {
          loaded_columns = plane.data.width;
          first_loaded = true;
        } else if (plane.data.width != loaded_columns) {
          std::cerr << "ERROR: Column mismatch. Plane " << key
                    << " has " << plane.data.width << " columns, expected "
                    << loaded_columns << std::endl;
          return false;
        }

        std::cout << "Loaded " << key << ": " << plane.data.width << "x"
                  << plane.data.height << std::endl;
      } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load " << key << " from " << image_path
                  << ": " << e.what() << std::endl;
        return false;
      }
    }
  }

  // Set common column count if not explicitly provided
  if (common_columns_ == 0 && first_loaded) {
    common_columns_ = loaded_columns;
  }

  // Load or generate remaining images
  for (auto& [key, plane] : planes_) {
    if (!plane.provided) {
      std::string placeholder = "[" + key + "]";
      if (!loadOrGenerateImage(plane, placeholder)) {
        return false;
      }

      // Verify loaded/generated image has same width
      if (first_loaded && plane.data.width != loaded_columns) {
        std::cerr << "ERROR: Generated/found " << key << " has " << plane.data.width
                  << " columns, expected " << loaded_columns << std::endl;
        return false;
      }
    }
  }

  if (args_.verbose) {
    std::cout << "All images processed. Common column count: " << common_columns_ << std::endl;
  }

  return true;
}

bool ImagesTopcap::generatePcap() {
  std::cout << "Generating PCAP file: " << args_.output_file << std::endl;

  try {
    // Construct the detchannelmaps instance from the selected plugin.
    // This populates the global_map used by build_wib_frame.
    TLOG_DEBUG(1) << "Loading channel map plugin: " << args_.plugin;
    try {
        global_map = dunedaq::detchannelmaps::make_map(args_.plugin);
    } catch (const std::exception& e) {
      std::cerr << "ERROR: Failed to load channel map plugin '" << args_.plugin
                << "': " << e.what() << std::endl;
      global_map.reset();
    }

    if (!global_map) {
      throw std::runtime_error("Channel map is required for offline-channel-based packet generation");
    }

    // Derive packet order from the channel map
    packet_order_ = buildPacketOrderMap(*global_map);

    // Build the wire-to-offline-channel mapping from the channel map plugin
    if (!buildWireMapping()) {
      throw std::runtime_error("Failed to build wire-to-channel mapping");
    }

    PcapWriter pcap(args_.output_file);

    // Print verbose info about wire mappings
    if (args_.verbose) {
      for (auto const& [key, plane] : planes_) {
        unsigned int plane_idx = getPlaneIndex(plane.plane_char);
        int map_key = static_cast<int>(plane_idx) * 10 + static_cast<int>(plane.tpc_num);
        auto it = wire_to_offline_.find(map_key);
        if (it != wire_to_offline_.end() && !it->second.empty()) {
          std::cout << "Plane " << key << ": " << it->second.size() << " wires mapped to offline channels "
                    << it->second.front() << "..." << it->second.back() << std::endl;
        }
      }
    }

    // ---- Build the full pixel data block indexed by [offline_channel][timetick] ----
    // pixelDataBlock[ch][col] = 16-bit ADC value for offline channel ch at timetick col.
    // Dimensions: [0..Nchannels-1][0..common_columns_-1]  (e.g. [1280][64])
    const unsigned int total_channels = total_offline_channels_;
    std::vector<std::vector<uint16_t>> pixelDataBlock(
        total_channels, std::vector<uint16_t>(common_columns_, 0));

    for (auto const& [key, plane] : planes_) {
      uint16_t const ncols = std::min(plane.data.width, common_columns_);
      unsigned int plane_idx = getPlaneIndex(plane.plane_char);
      int map_key = static_cast<int>(plane_idx) * 10 + static_cast<int>(plane.tpc_num);

      auto it = wire_to_offline_.find(map_key);
      if (it == wire_to_offline_.end()) {
        TLOG_DEBUG(1) << "No wire mapping for plane " << key << " - skipping";
        continue;
      }

      const auto& wire_vec = it->second;
      unsigned int wires_mapped = static_cast<unsigned int>(wire_vec.size());
      unsigned int wires_to_use = std::min(wires_mapped, static_cast<unsigned int>(plane.data.height));

      for (unsigned int wire = 0; wire < wires_to_use; ++wire) {
        raw::ChannelID_t const ch = wire_vec[wire];
        // Skip wires that don't have a mapped offline channel
        if (ch == std::numeric_limits<unsigned int>::max()) {
          continue;
        }
        TLOG_DEBUG(2) << "Mapping plane " << key << " wire " << std::setw(3) << wire
                      << " data[col=0] " << std::setw(4) << std::hex << plane.data.pixels[wire * plane.data.width]
                      << " to offline channel " << std::dec << ch;
        for (uint16_t col = 0; col < ncols; ++col) {
          pixelDataBlock[ch][col] = plane.data.pixels[wire * plane.data.width + col];
        }
      }

      if (wires_mapped != plane.data.height) {
        TLOG_DEBUG(1) << "Plane " << key << ": mapped " << wires_to_use << " of " << plane.data.height
                      << " image rows (channel map has " << wires_mapped << " wires)";
      }
    }

    // ---- BREAKPOINT: pixelDataBlock is fully populated here ----
    // All 6 images have been merged into pixelDataBlock[offline_channel][timetick].
    // Inspect pixelDataBlock[0..1279][0..common_columns_-1] to view the full detector snapshot.
    TLOG_DEBUG(1) << "pixelDataBlock populated: " << total_channels
                  << " channels x " << common_columns_ << " timeticks";

    // Debug logging: 32 timeticks per log line, one line per channel per window
    for (unsigned int ch = 0; ch < total_channels; ++ch) {
      TLOG_DEBUG_SCOPED(10) {
        TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 0<<":" << std::hex << std::setfill('0');
        for (uint16_t col = 0; col < 16; ++col)
          TLOG_ADD << " " << pixelDataBlock[ch][col];
      }
      TLOG_DEBUG_SCOPED(11) {
        TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 0<<":" << std::hex << std::setfill('0');
        for (uint16_t col = 16; col < 32; ++col)
          TLOG_ADD << " " << pixelDataBlock[ch][col];
      }
      TLOG_DEBUG_SCOPED(12) {
        TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 32<<":"<<std::hex << std::setfill('0');
        for (uint16_t col = 32; col < 64; ++col)
          TLOG_ADD << " " << pixelDataBlock[ch][col];
      }
      if (common_columns_ > 64) {
        TLOG_DEBUG_SCOPED(13) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 64<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 64; col < 96; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
        TLOG_DEBUG_SCOPED(14) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 96<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 96; col < 128; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
      }
      if (common_columns_ > 128) {
        TLOG_DEBUG_SCOPED(15) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 128<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 128; col < 160; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
        TLOG_DEBUG_SCOPED(16) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 160<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 160; col < 192; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
      }
      if (common_columns_ > 192) {
        TLOG_DEBUG_SCOPED(17) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 192<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 192; col < 224; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
        TLOG_DEBUG_SCOPED(18) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 224<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 224; col < 256; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
      }
      if (common_columns_ > 256) {
        TLOG_DEBUG_SCOPED(19) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]:" << 256<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 256; col < 288; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
        TLOG_DEBUG_SCOPED(20) {
          TLOG_ADD << "ch[" << std::setw(4) << ch << "]" << 288<<":" << std::hex << std::setfill('0');
          for (uint16_t col = 288; col < 320; ++col)
            TLOG_ADD << " " << pixelDataBlock[ch][col];
        }
      }
    }

    if (total_channels != 1280) {
      throw std::runtime_error("Expected 1280 offline channels in ICEBERG channel map");
    }
    if ((total_channels % kChannelsPerPacket) != 0) {
      throw std::runtime_error("Offline channel count is not divisible into WIB Ethernet packets");
    }
    if (!global_map) {
      throw std::runtime_error("Channel map is required for offline-channel-based packet generation");
    }

    constexpr uint16_t kPacketsPerGroup = 20;  // 1280 channels / 64 channels per packet
    constexpr unsigned int n_chan_per_stream = 64;

    uint16_t const num_column_groups = common_columns_ / kTicksPerPacket;
    uint16_t sequence_id = 0;

    for (uint16_t col_group = 0; col_group < num_column_groups; ++col_group) {
      uint16_t const tick_offset = col_group * kTicksPerPacket;
      uint64_t const frame_timestamp = static_cast<uint64_t>(col_group) * 2048u;

      // Initialize all 20 packets for this column group
      std::vector<WIBEthFrame> frames(kPacketsPerGroup);
      for (uint16_t pkt_idx = 0; pkt_idx < kPacketsPerGroup; ++pkt_idx) {
        WIBEthFrame& frame = frames[pkt_idx];
        frame = {};  // zero-initialize
        frame.daq_header.version = 1;
        frame.daq_header.det_id = 3;
        frame.daq_header.crate_id = 0;
        frame.daq_header.slot_id = 0;
        frame.daq_header.stream_id = 0;
        frame.daq_header.reserved = 0;
        frame.daq_header.seq_id = (sequence_id + pkt_idx) & 0x0FFFu;
        frame.daq_header.block_length = sizeof(WIBEthFrame) / sizeof(WIBEthFrame::word_t);
        frame.set_timestamp(frame_timestamp);
        frame.header.version = 1;
        frame.header.context = pkt_idx & 0xFFu;
        frame.header.ready = 1;
        frame.header.link_valid = 0x3u;
        frame.header.wib_sync = 1;
        frame.header.femb_sync = 0x3u;
        frame.header.colddata_timestamp_0 = frame_timestamp & 0x7FFFu;
        frame.header.colddata_timestamp_1 = (frame_timestamp >> 15) & 0x7FFFu;
        frame.header.extra_data = 0;
      }

      // Loop over offline channels and populate frames using channel map
      for (uint32_t off_chan = 0; off_chan < total_channels; ++off_chan) {
        auto coords = global_map->get_crate_slot_fiber_chan_from_offline_channel(off_chan);
        if (!coords.has_value()) {
          TLOG_ERROR() << "No hardware coordinates for offline channel " << off_chan;
          return false;
        }

        // Derive packet index, stream, and stream channel from coordinates
        unsigned int const out_stream_idx = (coords->channel / n_chan_per_stream) + (coords->fiber << 2);
        unsigned int const out_stream = ((coords->fiber & 0x1U) << 6) | ((coords->channel / n_chan_per_stream) & 0x3U);
        unsigned int const out_chan = coords->channel % n_chan_per_stream;

        uint32_t const key = encodeSlotStreamIdx(static_cast<uint16_t>(coords->slot), out_stream_idx);
        auto it = packet_order_.reverse.find(key);
        if (it == packet_order_.reverse.end()) {
          TLOG_ERROR() << "No packet index for slot=" << coords->slot
                       << " stream_idx=" << out_stream_idx
                       << " (offline channel " << off_chan << ")";
          return false;
        }
        unsigned int const packet_index = it->second;

        WIBEthFrame& frame = frames[packet_index];
        // Set header fields from first channel encountered in this packet
        // (crate/slot/stream are the same for all 64 channels in a packet)
        if (frame.daq_header.crate_id == 0 && frame.daq_header.slot_id == 0 && frame.daq_header.stream_id == 0) {
          frame.daq_header.crate_id = static_cast<uint16_t>(coords->crate);
          frame.daq_header.slot_id = static_cast<uint16_t>(coords->slot);
          frame.daq_header.stream_id = static_cast<uint16_t>(out_stream);
          frame.header.channel = static_cast<uint16_t>(out_chan);
        }

        TLOG_DEBUG(2) << "Mapping offline channel " << off_chan
                      << " (crate=" << coords->crate
                      << ", slot=" << coords->slot
                      << ", fiber=" << coords->fiber
                      << ", channel=" << coords->channel
                      << ") to packet_index=" << packet_index
                      << ", out_stream=" << out_stream
                      << ", out_chan=" << out_chan;

        // Fill ADC values for this channel across all time samples
        for (uint16_t sample = 0; sample < kTicksPerPacket; ++sample) {
          uint16_t const adc = clamp_to_14bit(pixelDataBlock[off_chan][tick_offset + sample]);
          if (sample == 0) {
            TLOG_DEBUG(3) << "  ADC values for offline channel " << off_chan
                          << " (packet_index=" << packet_index
                          << ", out_chan=" << out_chan << "): " << std::hex << std::setfill('0') << adc;
          }
          frame.set_adc(out_chan, sample, adc);
        }
      }

      // Write all 20 packets for this column group
      for (uint16_t pkt_idx = 0; pkt_idx < kPacketsPerGroup; ++pkt_idx) {
        uint16_t const packet_id = static_cast<uint16_t>(col_group * kPacketsPerGroup + pkt_idx);
        std::vector<uint8_t> const packet_data = build_network_packet(frames[pkt_idx], packet_id);

        TLOG_DEBUG(2) << "Writing frame for packet_id=" << packet_id
                      << " (col_group=" << col_group
                      << ", pkt_idx=" << pkt_idx
                      << ", tick_offset=" << tick_offset
                      << ", timestamp=" << frame_timestamp
                      << ", sequence_id=" << (sequence_id + pkt_idx) << ")";

        pcap.writePacket(packet_data.data(), packet_data.size(),10000000 + frame_timestamp);  // Use timestamp in nanoseconds
      }
      sequence_id += kPacketsPerGroup;
    }

    std::cout << "PCAP file generated successfully" << std::endl;
    return true;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to generate PCAP: " << e.what() << std::endl;
    return false;
  }
}

int main(int argc, char* argv[]) {
  setenv("TRACE_MSGMAX", "0", 1);
  TRACE_CNTL("reset");
  try {
    ImagesTopcap converter;

    if (!converter.parseArguments(argc, argv)) {
      return 1;
    }

    if (!converter.processImages()) {
      std::cerr << "Failed to process images" << std::endl;
      return 1;
    }

    if (!converter.generatePcap()) {
      std::cerr << "Failed to generate PCAP" << std::endl;
      return 1;
    }

    std::cout << "Success!" << std::endl;
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
