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
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <optional>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "TRACE/trace.h"
#include "PngImageLoader.hpp"
#include "PcapWriter.hpp"

// Mock types and structs (replace with real ones from the actual geometry)
namespace geo {
  struct WireID {
    unsigned int cryostat;
    unsigned int tpc;
    unsigned int plane;
    unsigned int wire;
    
    WireID(unsigned int cry, unsigned int t, unsigned int p, unsigned int w)
      : cryostat(cry), tpc(t), plane(p), wire(w) {}
  };
}

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
  std::string output_file = "output.pcap";
  bool verbose = false;
  uint16_t timestamp_us = 0;
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

  // Check that columns is a multiple of 64 and <= 512
  if (plane.data.width % 64 != 0 || plane.data.width > 512 || plane.data.width == 0) {
    std::cerr << "ERROR: Plane " << plane.plane_char << plane.tpc_num
              << " has invalid width: " << plane.data.width
              << " (must be multiple of 64, max 512)" << std::endl;
    return false;
  }

  return true;
}

std::string ImagesTopcap::buildImageFilename(char plane_char, 
                                             unsigned int tpc_num,
                                             uint16_t cols) const {
  std::string filename = std::string(1, plane_char) + std::to_string(tpc_num) + 
                        "x" + std::to_string(cols) + ".png";
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
        PngImageLoader::generateTestImage(filename, common_columns_, 
                                         plane.expected_rows);
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
                << "  --output FILE      Output PCAP file (default: 'output.pcap')" << std::endl
                << "  --columns N        Force column count (must be multiple of 64)" << std::endl
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
    else if (arg == "--output" && i + 1 < argc) {
      args_.output_file = argv[++i];
    }
    else if (arg == "--columns" && i + 1 < argc) {
      common_columns_ = std::atoi(argv[++i]);
      if (common_columns_ % 64 != 0 || common_columns_ > 512 || common_columns_ == 0) {
        std::cerr << "ERROR: --columns must be a multiple of 64 and at most 512" << std::endl;
        return false;
      }
    }
    else if (arg == "--verbose") {
      args_.verbose = true;
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
    PcapWriter pcap(args_.output_file);

    // For each group of 64 columns
    uint16_t num_column_groups = common_columns_ / 64;

    // In a real implementation, this would:
    // 1. For each column group and each row (wire):
    //    - Create WireID(cryostat=0, tpc, plane, wire)
    //    - Call DuneApaWireReadoutGeom::PlaneWireToChannel(wireid) to get offline channel
    //    - Call TPCChannelMap::get_crate_slot_fiber_chan_from_offline_channel(channel)
    //    - Build UDP packet with the wire data
    //    - Write to PCAP

    // For now, create placeholder packets
    for (uint16_t col_group = 0; col_group < num_column_groups; ++col_group) {
      // Create a simple test packet for each column group
      uint32_t packet_size = 1024;  // Example: 1KB packet
      std::vector<uint8_t> packet_data(packet_size);

      // Fill with test pattern (plane index in first 4 bytes)
      // In real implementation, this would contain actual wire channel data
      memset(packet_data.data(), 0xAA, packet_size);

      // Write packet to PCAP
      pcap.writePacket(packet_data.data(), packet_data.size());
    }

    std::cout << "PCAP file generated successfully" << std::endl;
    return true;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to generate PCAP: " << e.what() << std::endl;
    return false;
  }
}

int main(int argc, char* argv[]) {
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
