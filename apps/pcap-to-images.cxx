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
#include <cctype>

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
  
  // Pixel data block indexed by [offline_channel][timetick]
  // Same as pixelDataBlock in images-to-pcap.cxx
  std::vector<std::vector<uint16_t>> pixelDataBlock;
  
  uint32_t getPlaneIndex(char plane_char) const;
  std::string buildOutputFilename(char plane_char, unsigned int tpc_num) const;
  bool saveImage(const std::string& filename, const PlaneInfo& plane);
  void populatePixelDataBlockFromPackets();
  void mapPixelDataBlockToPlanes();

  geo::IcebergWireChannelMap channel_map_;
};

uint32_t PcapToImages::getPlaneIndex(char plane_char) const {
  if (plane_char == 'U') return 0;
  if (plane_char == 'V') return 1;
  if (plane_char == 'Z') return 2;
  throw std::runtime_error(std::string("Invalid plane character: ") + plane_char);
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

void PcapToImages::populatePixelDataBlockFromPackets() {
  // Initialize pixelDataBlock[offline_channel][timetick] array
  pixelDataBlock.assign(kTotalChannels, std::vector<uint16_t>(common_columns_, 0));

  if (!global_map) {
    std::cerr << "ERROR: Channel map is required for packet processing" << std::endl;
    return;
  }

  constexpr unsigned int n_chan_per_stream = 64;

  // Process each packet and populate pixelDataBlock
  for (uint32_t pkt_idx = 0; pkt_idx < header_ptrs_.size(); ++pkt_idx) {
    const auto* header = header_ptrs_[pkt_idx];
    if (!header) continue;

    const auto* frame = reinterpret_cast<const WIBEthFrame*>(header);

    // Get hardware coordinates from the frame header
    uint16_t crate = frame->daq_header.crate_id;
    uint16_t slot = frame->daq_header.slot_id;
    uint16_t stream = frame->daq_header.stream_id;

    // Decode fiber and channel group from stream
    // (reverse of: out_stream = ((fiber & 0x1U) << 6) | ((channel / 64) & 0x3U))
    unsigned int fiber = (stream >> 6) & 0x1U;
    unsigned int chan_group = stream & 0x3U;

    // Determine tick offset from packet index
    uint16_t col_group = static_cast<uint16_t>(pkt_idx / kPacketsPerGroup);
    uint16_t tick_offset = col_group * kTicksPerPacket;

    TLOG_DEBUG(2) << "Packet " << pkt_idx << ": crate=" << crate
                  << " slot=" << slot << " stream=0x" << std::hex << stream << std::dec
                  << " fiber=" << fiber << " chan_group=" << chan_group
                  << " tick_offset=" << tick_offset;

    // Process each of the 64 channels in this packet
    for (uint16_t stream_chan = 0; stream_chan < kChannelsPerPacket; ++stream_chan) {
      // Compute the hardware channel for the channel map lookup
      unsigned int map_channel = chan_group * n_chan_per_stream + stream_chan;

      // Look up offline channel from crate/slot/stream/channel
      uint32_t off_chan = global_map->get_offline_channel_from_crate_slot_stream_chan(
          crate, slot, stream, stream_chan);
      TLOG_DEBUG(3) << "(crate=" << crate << " slot=" << slot << " stream=" << stream
                    << " stream_chan=" << stream_chan << ")"
                    << " -> offline_channel " << off_chan;

      if (off_chan >= kTotalChannels) {
        TLOG_ERROR() << "Offline channel " << off_chan << " out of range for crate=" << crate
                      << " slot=" << slot << " fiber=" << fiber
                      << " map_channel=" << map_channel << " (stream_chan=" << stream_chan << ")";
        exit(EXIT_FAILURE);
      }

      // Extract ADC values for all time samples and store in pixelDataBlock
      for (uint16_t sample = 0; sample < kTicksPerPacket; ++sample) {
        uint16_t tick = tick_offset + sample;
        if (tick < common_columns_) {
          uint16_t adc = frame->get_adc(stream_chan, sample);
          if (tick ==0) TLOG_DEBUG(4) << "setting pixelDataBlock[offchan=" << off_chan << "]"
                          << "[tick=" << tick << "] = " << adc << " (from slot=" << slot
                          << " stream=" << stream << " stream_chan=" << stream_chan
                          << " sample=" << sample << ") [pkt_idx=" << pkt_idx << "]";
          pixelDataBlock[off_chan][tick] = adc;
        }
      }
    }
  }

  TLOG_DEBUG(1) << "pixelDataBlock populated: " << kTotalChannels
                << " channels x " << common_columns_ << " timeticks";

  // Debug logging: 32 timeticks per log line, one line per channel per window
  for (uint32_t ch = 0; ch < kTotalChannels; ++ch) {
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
}

void PcapToImages::mapPixelDataBlockToPlanes() {
  // Allocate pixel data for all planes
  for (auto& [key, plane] : planes_) {
    plane.width = common_columns_;
    plane.pixels.assign(static_cast<size_t>(plane.expected_rows) * common_columns_, 0);
  }

  // Loop over all offline channels and map to wire(s)
  for (uint32_t off_chan = 0; off_chan < kTotalChannels; ++off_chan) {
    // Get the wire(s) for this offline channel (may be multiple for wrapped wires)
    std::vector<geo::WireID> wire_ids = channel_map_.ChannelToWire(off_chan);

    if (wire_ids.empty()) {
      TLOG_DEBUG(3) << "No wire mapping for offline channel " << off_chan;
      continue;
    }

    // Copy timetick data to each wire position
    for (const auto& wire_id : wire_ids) {
      // Determine which plane this wire belongs to
      char plane_char;
      switch (wire_id.Plane) {
        case 0: plane_char = 'U'; break;
        case 1: plane_char = 'V'; break;
        case 2: plane_char = 'Z'; break;
        default:
          TLOG_DEBUG(1) << "Invalid plane " << wire_id.Plane << " for channel " << off_chan;
          continue;
      }

      // Build plane key
      std::string key = std::string(1, static_cast<char>(std::tolower(plane_char))) +
                        std::to_string(wire_id.TPC);

      auto it = planes_.find(key);
      if (it == planes_.end()) {
        TLOG_DEBUG(1) << "Plane " << key << " not found for channel " << off_chan;
        continue;
      }

      PlaneInfo& plane = it->second;
      unsigned int wire_num = wire_id.Wire;

      if (wire_num >= plane.expected_rows) {
        TLOG_DEBUG(1) << "Wire " << wire_num << " exceeds plane " << key
                      << " height " << plane.expected_rows;
        continue;
      }

      // Copy all timeticks for this channel to the wire row
      for (uint16_t tick = 0; tick < common_columns_; ++tick) {
        plane.pixels[static_cast<size_t>(wire_num) * common_columns_ + tick] = pixelDataBlock[off_chan][tick];
      }

      TLOG_DEBUG(4) << "Mapped offline channel " << off_chan 
                    << " to " << key << " wire " << wire_num;
    }
  }

  TLOG_DEBUG(1) << "All 6 plane pixel data blocks populated";
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
  TLOG() << "Processing packets...";

  if (!global_map) {
    std::cerr << "ERROR: Channel map is required for processing" << std::endl;
    return false;
  }

  // Step 1: Build pixelDataBlock[offline_channel][timetick] from all packets
  std::cout << "Building pixel data block from packets..." << std::endl;
  populatePixelDataBlockFromPackets();

  if (args_.verbose) {
    std::cout << "Populated pixelDataBlock[" << kTotalChannels << "][" 
              << common_columns_ << "]" << std::endl;
  }

  // Step 2: Loop over offline channels and map to wire positions in planes
  std::cout << "Mapping offline channels to wire positions..." << std::endl;
  mapPixelDataBlockToPlanes();
  
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
