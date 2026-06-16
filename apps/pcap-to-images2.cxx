/* pcap-to-images2:
   Like pcap-to-images.cxx, but this version will have a CUDA kernel (to
   parallelize data movement from packets to images) in mind.
   This version will take an --max-packets <N> option (with default 80,000)
   and a --columns <N> option (with default 256).
   Using these option values, it will allocate 5 blocks of memory:
   1. block for packet header data
   2. block for pointers to the packet header data
   3. block for pointers to the packet's adc_words data
   4. block for packet adc_words blocks
   5. block for image data. Num images = packets/20/(cols/64) * 6.
   Blocks 2 and 3 will eventually be given to the GPU. and block 5 is also
   a GPU block (which will be copied back from the GPU)

   The program should assume the packet pointer ordering below and build
   a memory lookup that will allow packet pixel data to be copied directly
   to one or two location is the image data blocks.

   As it reads the pcap, it will
   1. check that there are groups of 20 packets with DAQEthHeader
      information in the following order:
Pkt timestamp        crate slot stream UDPbyts
--- ---------------- ----- ---- ------ -------
  1 0000000000000000     8    2      0    7200
  2 0000000000000000     8    2      1    7200
  3 0000000000000000     8    2      2    7200
  4 0000000000000000     8    2      3    7200
  5 0000000000000000     8    2     64    7200
  6 0000000000000000     8    2     65    7200
  7 0000000000000000     8    2     66    7200
  8 0000000000000000     8    2     67    7200
  9 0000000000000000     8    3      0    7200
 10 0000000000000000     8    3      1    7200
 11 0000000000000000     8    3      2    7200
 12 0000000000000000     8    3      3    7200
 13 0000000000000000     8    3     64    7200
 14 0000000000000000     8    3     65    7200
 15 0000000000000000     8    3     66    7200
 16 0000000000000000     8    3     67    7200
 17 0000000000000000     8    4      0    7200
 18 0000000000000000     8    4      1    7200
 19 0000000000000000     8    4      2    7200
 20 0000000000000000     8    4      3    7200

    2. it will verify that each packet in the group of 20 has the same
        timestamp and that from group to group, the timestamps increment
        by 2048 (0x800).
    3. it will copy the packet ethernet, IP, UDP, DAQEthHeader, and WIBEthHeader
        contiguous part to one block of memory and record a ptr to that part
        in the header pointer block.
    4. it will copy the adc_words data from the pcap to the data block and
        record the ptr in the adc_words pointer block.

After all the data is read in the the quick lookup table is made, the program
should be able to loop through the packet adc_words data and create images in
the image memory block. Oh, the images in the memory block should be
in U0, U1, V0, V1, Z0, Z1 order.

After all the packets are process and the images made, the program and
loop through the image memory and create .png files.

pcap-to-images2 — Full Specification
Overview
Like pcap-to-images, but designed with a CUDA kernel in mind for parallelizing
data movement from packets to images. Uses a flat, procedural code structure (no class).
Memory is laid out in 5 contiguous raw-byte blocks suitable for GPU transfer.

Command-Line Options
Option	Default	Description
INPUT_PCAP	(required)	Positional argument: input PCAP file
--max-packets <N>	80000	Maximum packets to read from PCAP
--columns <N>	256	Image width in timeticks. Sets the group size: group_size = 20 * (columns / 64)
--output-dir DIR	.	Directory for output images
--output-prefix STR	""	Prefix prepended to output filenames
--plugin NAME	ICEBERGChannelMap	Channel map plugin name
--max-png <N>	24	Maximum PNG images to write (must be multiple of 6)
--start-group <N>	0	First image group to write (selectable range)
--gpu	off	Use GPU path (placeholder; CPU path is default)
--verbose	off	Enable verbose output
Derived Values
Packets per image group = 20 * (columns / 64). E.g., columns=256 → 80 packets/group.
Total image groups = total_packets / packets_per_group
Total images = total_image_groups * 6
PNGs written = min(max_png, total_images), starting from --start-group
Memory Blocks (5 blocks, all raw uint8_t* via malloc/new)
Block 1 — Packet header data: Single contiguous allocation. Per packet: 74 bytes (Ethernet 14B + IPv4 20B + UDP 8B + DAQEthHeader 16B + WIBEthHeader 16B). Total: max_packets * 74 bytes.
Block 2 — Header byte-offsets: Array of byte offsets (not pointers) into Block 1. Each entry is the offset from Block 1's start to that packet's header data. Total: max_packets * sizeof(size_t) bytes.
Block 3 — ADC byte-offsets: Array of byte offsets into Block 4. Each entry is the offset from Block 4's start to that packet's adc_words data. Total: max_packets * sizeof(size_t) bytes.
Block 4 — Packet ADC data: Single contiguous allocation. Per packet: 7168 bytes (adc_words[64][14]). Copied contiguously from each packet. Total: max_packets * 7168 bytes.
Block 5 — Image data: 16-bit (uint16_t) pixels. All image groups allocated at once. Per image group: 6 images with rows {316, 316, 315, 315, 240, 240} and width = --columns. Total pixels: total_image_groups * (316+316+315+315+240+240) * columns. Total bytes: total_pixels * 2.
Lookup Table
Built on CPU after the first group of 20 packets is read (to confirm crate/slot/stream values).
1280 entries (20 packets × 64 channels per packet).
Key: (packet_index_within_group, stream_chan) — flattened to a 1D index.
Value: 2 destinations (fixed). Each destination: (image_index [0-5], row, col_offset).
Second destination set to (-1, -1, -1) for non-wrapped wires.
Goal: Map directly from (crate, slot, stream) → wire position(s) without going through offline channels,
if the channel map supports it.
Used on CPU in default mode (--cpu), copied to GPU when --gpu is specified.
Packet Validation
Ordering: Enforce the exact pattern from the spec: crate=8, slot={2,3,4},
          stream={0,1,2,3,64,65,66,67} in the specified order. Fatal error if any packet deviates.
Timestamps: All 20 packets in a group must share the same timestamp.
            Timestamps must increment by 2048 (0x800) between consecutive groups. Fatal error on any mismatch.
Validation scope: Every group is validated (all groups, not just the first).
Undersized packets: Fatal error if any packet is smaller than 7242 bytes (42 network headers + 7200 WIBEthFrame).
Processing Pipeline
Read entire PCAP into Blocks 1–4 using extended PcapReader (add a bulk-read
method to existing PcapReader while keeping backward compatibility).
Validate all groups (ordering + timestamps).
Build lookup table from first group's header data + channel map.
Process: For each image group, use the lookup table to extract 14-bit ADC
         values from packed adc_words (Block 4) and scatter them to image
         pixel locations (Block 5). On CPU by default; --gpu flag is a
         placeholder for a future CUDA kernel.
Write PNGs: Write up to --max-png images starting from --start-group, using
            same PNG output as pcap-to-images.
CUDA Design
File structure: Main logic in .cxx, CUDA kernel in a companion .cu file linked together.
Kernel workload: ADC extraction (unpack 14-bit values from packed adc_words)
                 + scatter to image pixel locations using the lookup table.
--gpu flag: Placeholder in initial implementation. CPU path is default.
Blocks 2, 3 (byte-offsets) and Block 5 (image data) are designed for GPU
transfer. Block 5 is copied back from GPU.
Output
Format: PNG files (16-bit grayscale), same as pcap-to-images.
Naming: {prefix}{plane}{tpc}x{columns}.png (e.g., U0x256.png), with group index appended for multi-group output.
Plane dimensions: U=316 rows, V=315 rows, Z=240 rows (same as pcap-to-images). Width = --columns.
Logging
Uses TRACE/TLOG system, identical to pcap-to-images.
Channel Map
Uses --plugin (default ICEBERGChannelMap) via detchannelmaps::make_map().
Goal is a direct crate/slot/stream → wire position(s) mapping in the lookup
table, bypassing offline channel numbers if possible. If TPCChannelMap doesn't
support this directly, both TPCChannelMap and IcebergWireChannelMap will be needed (as in v1).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include "TRACE/trace.h"
#include "detdataformats/DAQEthHeader.hpp"
#include "fddetdataformats/WIBEthFrame.hpp"
#include "IcebergWireChannelMap.hpp"
#include "PngImageLoader.hpp"
#include "PcapReader.hpp"
#include "detchannelmaps/TPCChannelMap.hpp"

// ---------------------------------------------------------------------------
// GPU kernel entry point (defined in pcap-to-images2-kernel.cu)
// ---------------------------------------------------------------------------
#ifdef HAVE_ICEBERG_GPU
extern "C" void scatter_adc_to_images_gpu(
    const uint8_t* adc_block,
    const size_t*  adc_offsets,
    const int32_t* lookup,
    uint16_t*      image_block,
    uint32_t       packets_per_image_group,
    uint32_t       num_image_groups,
    uint16_t       columns,
    uint32_t       pixels_per_image_set);
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

using dunedaq::fddetdataformats::WIBEthFrame;

static constexpr uint16_t kChannelsPerPacket  = WIBEthFrame::s_num_channels;           // 64
static constexpr uint16_t kTicksPerPacket     = WIBEthFrame::s_time_samples_per_frame; // 64
static constexpr uint16_t kBitsPerAdc         = WIBEthFrame::s_bits_per_adc;           // 14
static constexpr uint16_t kAdcWordsPerTs      = WIBEthFrame::s_num_adc_words_per_ts;   // 14
static constexpr uint16_t kMax14BitAdc        = (1u << kBitsPerAdc) - 1u;

// Network header sizes
static constexpr size_t kEthernetHeaderSize = 14;
static constexpr size_t kIPv4HeaderSize     = 20;
static constexpr size_t kUDPHeaderSize      = 8;
static constexpr size_t kNetHeaderSize      = kEthernetHeaderSize + kIPv4HeaderSize + kUDPHeaderSize; // 42

// Frame component sizes
static constexpr size_t kDAQEthHeaderSize   = 16;  // sizeof(DAQEthHeader)
static constexpr size_t kWIBEthHeaderSize   = 16;  // sizeof(WIBEthHeader)
static constexpr size_t kFrameHeaderSize    = kDAQEthHeaderSize + kWIBEthHeaderSize;   // 32
static constexpr size_t kAdcBytesPerPacket  = sizeof(WIBEthFrame::word_t) *
                                              WIBEthFrame::s_time_samples_per_frame *
                                              WIBEthFrame::s_num_adc_words_per_ts;     // 7168

// Per-packet header in Block 1: network headers + DAQEthHeader + WIBEthHeader
static constexpr size_t kHeaderBytesPerPkt  = kNetHeaderSize + kFrameHeaderSize;       // 74

// ICEBERG detector constants
static constexpr uint32_t kTotalChannels    = 1280;
static constexpr uint16_t kPacketsPerGroup  = kTotalChannels / kChannelsPerPacket;     // 20

// Plane row counts (same as pcap-to-images)
static constexpr uint16_t kRowsU = 316;
static constexpr uint16_t kRowsV = 315;
static constexpr uint16_t kRowsZ = 240;
static constexpr uint32_t kRowsPerImageSet  = 2*kRowsU + 2*kRowsV + 2*kRowsZ;        // 1752

// Expected packet ordering within a group of 20 (crate, slot, stream)
struct PacketPattern {
  uint16_t crate;
  uint16_t slot;
  uint16_t stream;
};

static constexpr PacketPattern kExpectedOrder[20] = {
  {8, 2,  0}, {8, 2,  1}, {8, 2,  2}, {8, 2,  3},
  {8, 2, 64}, {8, 2, 65}, {8, 2, 66}, {8, 2, 67},
  {8, 3,  0}, {8, 3,  1}, {8, 3,  2}, {8, 3,  3},
  {8, 3, 64}, {8, 3, 65}, {8, 3, 66}, {8, 3, 67},
  {8, 4,  0}, {8, 4,  1}, {8, 4,  2}, {8, 4,  3},
};

// Lookup table dimensions.
// 1280 entries (20 packets x 64 channels), each with 2 destinations,
// each destination has 3 int32_t fields: (image_idx, row, col_offset).
// Invalid destination: image_idx == -1.
static constexpr int kLookupEntries     = kPacketsPerGroup * kChannelsPerPacket; // 1280
static constexpr int kDestsPerEntry     = 2;
static constexpr int kFieldsPerDest     = 3;  // image_idx, row, col_offset
static constexpr int kLookupTotalInts   = kLookupEntries * kDestsPerEntry * kFieldsPerDest;

// Image indices within a set of 6: 0=U0, 1=U1, 2=V0, 3=V1, 4=Z0, 5=Z1
static constexpr int kImagesPerSet = 6;

// ---------------------------------------------------------------------------
// Command-line arguments
// ---------------------------------------------------------------------------

struct Args {
  std::string input_file;
  std::string output_dir    = ".";
  std::string output_prefix;
  std::string plugin        = "ICEBERGChannelMap";
  uint32_t    max_packets   = 80000;
  uint32_t    min_packets   = 20;
  uint16_t    columns       = 256;
  uint32_t    max_png       = 12;
  uint32_t    start_group   = 0;
  bool        use_gpu       = false;
  bool        verbose       = false;
};

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

static void print_usage(const char* prog) {
  std::cout
    << "Usage: " << prog << " [OPTIONS] INPUT_PCAP\n"
    << "\n"
    << "Convert PCAP file to sets of 6 PNG images (U0,U1,V0,V1,Z0,Z1)\n"
    << "with CUDA-ready memory layout.\n"
    << "\n"
    << "Options:\n"
    << "  --max-packets <N>   Max packets to read/generate (default: 80000)\n"
    << "  --min-packets <N>   Min packets required (default: 20). If the PCAP\n"
    << "                      has fewer, the last group of 20 is repeated with\n"
    << "                      adjusted timestamps to reach this count.\n"
    << "                      Must be a multiple of the packet group size.\n"
    << "  --columns <N>       Image width in timeticks (default: 256)\n"
    << "                      Must be a positive multiple of 64.\n"
    << "  --output-dir DIR    Directory for output images (default: '.')\n"
    << "  --output-prefix STR Prefix for output filenames\n"
    << "  --plugin NAME       Channel map plugin (default: 'ICEBERGChannelMap')\n"
    << "  --max-png <N>       Max PNG images to write (default: 24, multiple of 6)\n"
    << "  --start-group <N>   First image group to write (default: 0)\n"
    << "  --gpu               Use GPU path (placeholder, CPU is default)\n"
    << "  --verbose           Enable verbose output\n"
    << "  -h, --help          Show this help message\n"
    << "\n"
    << "Packets per image group = 20 * (columns / 64).\n"
    << "E.g. columns=256 -> 80 packets/group, 6 images/group.\n";
}

static bool parse_args(int argc, char* argv[], Args& args) {
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return false;
    }
    else if (arg == "--max-packets" && i + 1 < argc) {
      args.max_packets = static_cast<uint32_t>(std::atol(argv[++i]));
    }
    else if (arg == "--min-packets" && i + 1 < argc) {
      args.min_packets = static_cast<uint32_t>(std::atol(argv[++i]));
    }
    else if (arg == "--columns" && i + 1 < argc) {
      args.columns = static_cast<uint16_t>(std::atoi(argv[++i]));
    }
    else if (arg == "--output-dir" && i + 1 < argc) {
      args.output_dir = argv[++i];
    }
    else if (arg == "--output-prefix" && i + 1 < argc) {
      args.output_prefix = argv[++i];
    }
    else if (arg == "--plugin" && i + 1 < argc) {
      args.plugin = argv[++i];
    }
    else if (arg == "--max-png" && i + 1 < argc) {
      args.max_png = static_cast<uint32_t>(std::atol(argv[++i]));
    }
    else if (arg == "--start-group" && i + 1 < argc) {
      args.start_group = static_cast<uint32_t>(std::atol(argv[++i]));
    }
    else if (arg == "--gpu") {
      args.use_gpu = true;
    }
    else if (arg == "--verbose") {
      args.verbose = true;
    }
    else if (arg[0] != '-') {
      if (args.input_file.empty()) {
        args.input_file = arg;
      } else {
        std::cerr << "ERROR: Multiple input files specified\n";
        return false;
      }
    }
    else {
      std::cerr << "ERROR: Unknown argument: " << arg << "\n";
      return false;
    }
  }

  if (args.input_file.empty()) {
    std::cerr << "ERROR: No input PCAP file specified\n";
    return false;
  }
  if (args.columns == 0 || args.columns % 64 != 0) {
    std::cerr << "ERROR: --columns must be a positive multiple of 64 (got "
              << args.columns << ")\n";
    return false;
  }
  if (args.max_png % kImagesPerSet != 0) {
    std::cerr << "ERROR: --max-png must be a multiple of " << kImagesPerSet
              << " (got " << args.max_png << ")\n";
    return false;
  }
  if (args.min_packets > args.max_packets) {
    std::cerr << "ERROR: --min-packets (" << args.min_packets
              << ") exceeds --max-packets (" << args.max_packets << ")\n";
    return false;
  }
  return true;
}

/// Row offset of image `image_idx` within one contiguous set of 6 images.
/// Images are laid out: U0(316), U1(316), V0(315), V1(315), Z0(240), Z1(240).
static uint32_t image_row_base(int image_idx) {
  switch (image_idx) {
    case 0: return 0;                                          // U0
    case 1: return kRowsU;                                     // U1
    case 2: return 2u * kRowsU;                                // V0
    case 3: return 2u * kRowsU + kRowsV;                       // V1
    case 4: return 2u * kRowsU + 2u * kRowsV;                  // Z0
    case 5: return 2u * kRowsU + 2u * kRowsV + kRowsZ;         // Z1
    default: return 0;
  }
}

static uint16_t rows_for_image(int image_idx) {
  switch (image_idx) {
    case 0: case 1: return kRowsU;
    case 2: case 3: return kRowsV;
    case 4: case 5: return kRowsZ;
    default: return 0;
  }
}

static char plane_char_for_image(int image_idx) {
  switch (image_idx) {
    case 0: case 1: return 'U';
    case 2: case 3: return 'V';
    case 4: case 5: return 'Z';
    default: return '?';
  }
}

static unsigned int tpc_for_image(int image_idx) {
  return static_cast<unsigned int>(image_idx & 1);
}

// ---------------------------------------------------------------------------
// Build lookup table
// ---------------------------------------------------------------------------
// Maps (pkt_index_within_group [0..19], stream_chan [0..63]) to up to 2
// image destinations: (image_index [0..5], wire_row, col_offset).
//
// Layout as a flat int32_t array:
//   lookup[ (pkt_idx*64 + stream_chan) * 6 + dest*3 + field ]
//   field 0 = image_idx, field 1 = row, field 2 = col_offset (always 0;
//   column position comes from the time sample index at scatter time).

static bool build_lookup_table(int32_t* lookup,
                                const std::string& plugin_name) {
  // Load the detchannelmaps plugin for crate/slot/stream -> offline channel
  std::shared_ptr<dunedaq::detchannelmaps::TPCChannelMap> chan_map;
  try {
    chan_map = dunedaq::detchannelmaps::make_map(plugin_name);
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to load channel map plugin '" << plugin_name
              << "': " << e.what() << "\n";
    return false;
  }
  if (!chan_map) {
    std::cerr << "ERROR: Channel map plugin returned null\n";
    return false;
  }

  // IcebergWireChannelMap for offline_channel -> (plane, tpc, wire)
  geo::IcebergWireChannelMap geom_map;

  // Initialise every entry to "invalid" (-1)
  for (int i = 0; i < kLookupTotalInts; ++i) {
    lookup[i] = -1;
  }

  for (int pkt_idx = 0; pkt_idx < kPacketsPerGroup; ++pkt_idx) {
    uint16_t crate  = kExpectedOrder[pkt_idx].crate;
    uint16_t slot   = kExpectedOrder[pkt_idx].slot;
    uint16_t stream = kExpectedOrder[pkt_idx].stream;

    for (uint16_t stream_chan = 0; stream_chan < kChannelsPerPacket; ++stream_chan) {

      // crate/slot/stream/stream_chan -> offline channel
      uint32_t off_chan = chan_map->get_offline_channel_from_crate_slot_stream_chan(
          crate, slot, stream, stream_chan);

      if (off_chan >= kTotalChannels) {
        TLOG_ERROR() << "Offline channel " << off_chan
                     << " out of range for pkt_idx=" << pkt_idx
                     << " stream_chan=" << stream_chan;
        return false;
      }

      // offline channel -> wire(s)  (1 or 2 for wrapped wires)
      std::vector<geo::WireID> wire_ids = geom_map.ChannelToWire(off_chan);

      int entry_base = (pkt_idx * kChannelsPerPacket + stream_chan)
                       * kDestsPerEntry * kFieldsPerDest;

      for (size_t d = 0; d < wire_ids.size() && d < static_cast<size_t>(kDestsPerEntry); ++d) {
        const auto& wid = wire_ids[d];
        int image_idx = -1;
        switch (wid.Plane) {
          case 0: image_idx = (wid.TPC == 0) ? 0 : 1; break; // U0 / U1
          case 1: image_idx = (wid.TPC == 0) ? 2 : 3; break; // V0 / V1
          case 2: image_idx = (wid.TPC == 0) ? 4 : 5; break; // Z0 / Z1
          default: continue;
        }

        uint16_t max_rows = rows_for_image(image_idx);
        if (wid.Wire >= max_rows) {
          TLOG_DEBUG(1) << "Wire " << wid.Wire << " exceeds rows " << max_rows
                        << " for image " << image_idx;
          continue;
        }

        int dest_base = entry_base + static_cast<int>(d) * kFieldsPerDest;
        lookup[dest_base + 0] = image_idx;
        lookup[dest_base + 1] = static_cast<int32_t>(wid.Wire);
        lookup[dest_base + 2] = 0; // col_offset: unused; time sample index provides column
      }

      TLOG_DEBUG(4) << "Lookup pkt=" << pkt_idx << " ch=" << stream_chan
                    << " off_chan=" << off_chan
                    << " dest0=(" << lookup[entry_base+0] << ","
                    << lookup[entry_base+1] << ")"
                    << " dest1=(" << lookup[entry_base+3] << ","
                    << lookup[entry_base+4] << ")";
    }
  }

  TLOG_DEBUG(1) << "Lookup table built: " << kLookupEntries << " entries";
  return true;
}

// ---------------------------------------------------------------------------
// Pad packets to reach min_packets by repeating the last group of 20
// ---------------------------------------------------------------------------
// Duplicates both header (Block 1) and ADC (Block 4) data from the last
// complete group of 20 packets, adjusting the DAQEthHeader timestamp in
// each padded header to continue the +0x800 sequence.
// Updates Blocks 2 and 3 (offset arrays) for the new entries.
// Returns the new total packet count.

static uint32_t pad_to_min_packets(
    uint8_t*  header_block,
    size_t*   header_offsets,
    uint8_t*  adc_block,
    size_t*   adc_offsets,
    uint32_t  current_packets,
    uint32_t  min_packets,
    uint32_t  max_packets) {

  if (current_packets >= min_packets) {
    return current_packets;
  }

  // The source group is the last complete group of 20 in the real data.
  // We need at least 20 real packets to have something to copy.
  if (current_packets < kPacketsPerGroup) {
    std::cerr << "ERROR: Cannot pad — need at least " << kPacketsPerGroup
              << " real packets, got " << current_packets << "\n";
    return current_packets;
  }

  // Identify the last complete group of 20
  uint32_t last_group_start = ((current_packets / kPacketsPerGroup) - 1) * kPacketsPerGroup;

  // Get the timestamp of the last group to continue the sequence
  const auto* last_daq_hdr = reinterpret_cast<const dunedaq::detdataformats::DAQEthHeader*>(
      header_block + header_offsets[last_group_start] + kNetHeaderSize);
  uint64_t last_timestamp = last_daq_hdr->get_timestamp();

  // How many groups-of-20 have been read so far?
  uint32_t existing_groups = current_packets / kPacketsPerGroup;

  uint32_t target = std::min(min_packets, max_packets);
  // Round target up to a multiple of kPacketsPerGroup
  if (target % kPacketsPerGroup != 0) {
    target = ((target + kPacketsPerGroup - 1) / kPacketsPerGroup) * kPacketsPerGroup;
  }
  if (target > max_packets) {
    target = (max_packets / kPacketsPerGroup) * kPacketsPerGroup;
  }

  TLOG() << "Padding from " << current_packets << " to " << target
            << " packets (repeating last group of 20)\n";

  uint32_t pkt = current_packets;
  uint32_t groups_added = 0;

  while (pkt + kPacketsPerGroup <= target) {
    // Timestamp for this new group
    uint64_t new_ts = last_timestamp + static_cast<uint64_t>(existing_groups + groups_added) * 0x800u
                      - last_timestamp  // cancel out to get absolute
                      + last_timestamp; // ... which simplifies to:
    // Actually: the Nth group (0-indexed) has timestamp = group0_ts + N * 0x800.
    // existing_groups groups are already present, so the next group index is
    // (existing_groups + groups_added).
    new_ts = last_timestamp + static_cast<uint64_t>(1 + groups_added) * 0x800u;

    for (uint16_t p = 0; p < kPacketsPerGroup; ++p) {
      uint32_t src_pkt = last_group_start + p;
      uint32_t dst_pkt = pkt + p;

      // Copy header block entry
      size_t dst_h_off = static_cast<size_t>(dst_pkt) * kHeaderBytesPerPkt;
      header_offsets[dst_pkt] = dst_h_off;
      std::memcpy(header_block + dst_h_off,
                   header_block + header_offsets[src_pkt],
                   kHeaderBytesPerPkt);

      // Patch the timestamp in the copied DAQEthHeader
      auto* dst_daq_hdr = reinterpret_cast<dunedaq::detdataformats::DAQEthHeader*>(
          header_block + dst_h_off + kNetHeaderSize);
      dst_daq_hdr->timestamp = new_ts;

      // Copy ADC block entry
      size_t dst_a_off = static_cast<size_t>(dst_pkt) * kAdcBytesPerPacket;
      adc_offsets[dst_pkt] = dst_a_off;
      std::memcpy(adc_block + dst_a_off,
                   adc_block + adc_offsets[src_pkt],
                   kAdcBytesPerPacket);
    }

    pkt += kPacketsPerGroup;
    ++groups_added;
  }

  TLOG() << "Padded " << groups_added << " groups (" << (groups_added * kPacketsPerGroup)
            << " packets), total now " << pkt << "\n";

  return pkt;
}

// ---------------------------------------------------------------------------
// Validate packet groups
// ---------------------------------------------------------------------------

static bool validate_packets(const uint8_t* header_block,
                              const size_t*  header_offsets,
                              uint32_t total_packets,
                              bool verbose) {
  if (total_packets == 0) {
    std::cerr << "ERROR: No packets read\n";
    return false;
  }
  if (total_packets % kPacketsPerGroup != 0) {
    std::cerr << "ERROR: Total packets (" << total_packets
              << ") is not a multiple of " << kPacketsPerGroup << "\n";
    return false;
  }

  uint32_t num_groups_of_20 = total_packets / kPacketsPerGroup;
  uint64_t prev_timestamp = 0;
  bool first_group = true;

  for (uint32_t g = 0; g < num_groups_of_20; ++g) {
    uint64_t group_timestamp = 0;

    for (uint16_t p = 0; p < kPacketsPerGroup; ++p) {
      uint32_t pkt_idx = g * kPacketsPerGroup + p;
      const uint8_t* hdr_ptr = header_block + header_offsets[pkt_idx];

      // DAQEthHeader starts after network headers in the Block 1 entry
      const auto* daq_hdr = reinterpret_cast<const dunedaq::detdataformats::DAQEthHeader*>(
          hdr_ptr + kNetHeaderSize);

      uint16_t crate  = daq_hdr->crate_id;
      uint16_t slot   = daq_hdr->slot_id;
      uint16_t stream = daq_hdr->stream_id;
      uint64_t ts     = daq_hdr->get_timestamp();

      // Verify crate/slot/stream against expected pattern
      const auto& exp = kExpectedOrder[p];
      if (crate != exp.crate || slot != exp.slot || stream != exp.stream) {
        std::cerr << "ERROR: Packet " << pkt_idx
                  << " (group " << g << ", pos " << p << ")"
                  << " crate=" << crate << " slot=" << slot
                  << " stream=" << stream
                  << " expected crate=" << exp.crate
                  << " slot=" << exp.slot
                  << " stream=" << exp.stream << "\n";
        return false;
      }

      // Verify timestamp consistency within group
      if (p == 0) {
        group_timestamp = ts;
      } else if (ts != group_timestamp) {
        std::cerr << "ERROR: Packet " << pkt_idx
                  << " (group " << g << ", pos " << p << ")"
                  << " timestamp=0x" << std::hex << ts
                  << " expected=0x" << group_timestamp << std::dec << "\n";
        return false;
      }

      if (verbose && p == 0) {
        std::cout << "Group " << std::setw(6) << g
                  << " timestamp=0x" << std::hex << std::setfill('0')
                  << std::setw(16) << group_timestamp
                  << std::dec << std::setfill(' ') << "\n";
      }
    }

    // Verify timestamp increment between consecutive groups
    if (!first_group) {
      uint64_t expected_ts = prev_timestamp + 0x800u;
      if (group_timestamp != expected_ts) {
        std::cerr << "ERROR: Group " << g << " timestamp=0x" << std::hex
                  << group_timestamp << " expected=0x" << expected_ts
                  << std::dec << "\n";
        return false;
      }
    }

    prev_timestamp = group_timestamp;
    first_group = false;
  }

  TLOG() << "Validated " << num_groups_of_20 << " groups of " << kPacketsPerGroup
            << " packets OK\n";
  return true;
}

// ---------------------------------------------------------------------------
// Extract a 14-bit ADC from packed adc_words (mirrors WIBEthFrame::get_adc)
// ---------------------------------------------------------------------------
// adc_data: points to start of adc_words for one packet (7168 bytes).
// channel: [0..63], sample: [0..63].

static inline uint16_t extract_adc(const uint8_t* adc_data,
                                    uint16_t channel,
                                    uint16_t sample) {
  // Each time sample occupies kAdcWordsPerTs (14) x uint64_t words.
  const auto* words = reinterpret_cast<const uint64_t*>(
      adc_data + static_cast<size_t>(sample) * kAdcWordsPerTs * sizeof(uint64_t));

  uint32_t bit_pos  = static_cast<uint32_t>(channel) * kBitsPerAdc;
  uint32_t word_idx = bit_pos / 64u;
  uint32_t bit_off  = bit_pos % 64u;

  uint64_t w0 = words[word_idx];

  if (bit_off + kBitsPerAdc <= 64u) {
    return static_cast<uint16_t>((w0 >> bit_off) & kMax14BitAdc);
  } else {
    uint32_t bits_lo = 64u - bit_off;
    uint64_t w1 = words[word_idx + 1];
    uint64_t val = (w0 >> bit_off) | (w1 << bits_lo);
    return static_cast<uint16_t>(val & kMax14BitAdc);
  }
}

// ---------------------------------------------------------------------------
// CPU scatter: extract ADC values and write to image block
// ---------------------------------------------------------------------------
// For each image group (packets_per_image_group packets), iterate over
// sub-groups of 20 packets.  Within each sub-group use the lookup table
// to map (pkt_in_20, stream_chan) -> image destination(s) and write the
// 64 time-samples' ADC values to the correct pixel positions.

static void scatter_adc_to_images_cpu(
    const uint8_t*  adc_block,
    const size_t*   adc_offsets,
    const int32_t*  lookup,
    uint16_t*       image_block,
    uint32_t        packets_per_image_group,
    uint32_t        num_image_groups,
    uint16_t        columns,
    uint32_t        pixels_per_image_set) 
{
  TLOG_DEBUG(1) << "packets_per_image_group="<<packets_per_image_group
                << " num_image_groups="<<num_image_groups
                << " pixels_per_image_set="<<pixels_per_image_set;
  uint32_t sub_groups_per_image = packets_per_image_group / kPacketsPerGroup;

  for (uint32_t ig = 0; ig < num_image_groups; ++ig) {
    uint32_t group_pkt_start      = ig * packets_per_image_group;
    uint32_t image_set_pixel_base = ig * pixels_per_image_set;

    for (uint32_t sg = 0; sg < sub_groups_per_image; ++sg) {
      uint16_t col_base = static_cast<uint16_t>(sg * kTicksPerPacket);

      for (uint16_t pkt_in_20 = 0; pkt_in_20 < kPacketsPerGroup; ++pkt_in_20) {
        uint32_t global_pkt = group_pkt_start + sg * kPacketsPerGroup + pkt_in_20;
        const uint8_t* adc_data = adc_block + adc_offsets[global_pkt];

        for (uint16_t ch = 0; ch < kChannelsPerPacket; ++ch) {
          int lk_base = (pkt_in_20 * kChannelsPerPacket + ch)
                        * kDestsPerEntry * kFieldsPerDest;

          for (int d = 0; d < kDestsPerEntry; ++d) {
            int32_t img_idx = lookup[lk_base + d * kFieldsPerDest + 0];
            int32_t row     = lookup[lk_base + d * kFieldsPerDest + 1];
            if (img_idx < 0) continue;

            uint32_t row_in_set = image_row_base(img_idx) + static_cast<uint32_t>(row);

            for (uint16_t sample = 0; sample < kTicksPerPacket; ++sample) {
              uint16_t col = col_base + sample;
              if (col >= columns) continue;

              uint16_t adc = extract_adc(adc_data, ch, sample);
              uint32_t px  = image_set_pixel_base + row_in_set * columns + col;
              image_block[px] = adc;
            }
          }
        }
      }
    }

    TLOG_DEBUG(2) << "Scattered image group " << ig;
  }
}

// ---------------------------------------------------------------------------
// Write PNG images from Block 5
// ---------------------------------------------------------------------------

static bool write_png_images(const uint16_t* image_block,
                              uint32_t num_image_groups,
                              uint16_t columns,
                              uint32_t pixels_per_image_set,
                              uint32_t start_group,
                              uint32_t max_png,
                              const std::string& output_dir,
                              const std::string& output_prefix) {
  if (!output_dir.empty() && output_dir != ".") {
    std::filesystem::create_directories(output_dir);
  }

  uint32_t max_groups_to_write = max_png / kImagesPerSet;
  uint32_t end_group = std::min(start_group + max_groups_to_write, num_image_groups);

  if (start_group >= num_image_groups) {
    std::cerr << "ERROR: --start-group " << start_group
              << " exceeds total groups " << num_image_groups << "\n";
    return false;
  }

  TLOG() << "Writing PNG images for groups " << start_group
            << ".." << (end_group - 1) << " ("
            << (end_group - start_group) * kImagesPerSet << " images)\n";

  for (uint32_t ig = start_group; ig < end_group; ++ig) {
    uint32_t set_pixel_base = ig * pixels_per_image_set;

    for (int img = 0; img < kImagesPerSet; ++img) {
      char     pc   = plane_char_for_image(img);
      unsigned tpc  = tpc_for_image(img);
      uint16_t rows = rows_for_image(img);
      uint32_t rbase = image_row_base(img);

      // Build filename: {prefix}{plane}{tpc}x{columns}[_gNNNN].png
      std::ostringstream fname;
      fname << output_dir << "/" << output_prefix
            << pc << tpc << "x" << columns;
      if (num_image_groups > 1) {
        fname << "_g" << std::setw(4) << std::setfill('0') << ig;
      }
      fname << ".png";

      // Copy this image's pixels out of the contiguous set
      std::vector<uint16_t> pixels(static_cast<size_t>(rows) * columns);
      for (uint16_t r = 0; r < rows; ++r) {
        uint32_t src = set_pixel_base + (rbase + r) * columns;
        std::memcpy(&pixels[static_cast<size_t>(r) * columns],
                     &image_block[src],
                     columns * sizeof(uint16_t));
      }

      try {
        PngImageLoader::saveImage(fname.str(), columns, rows, pixels);
        TLOG_DEBUG(1) << "Wrote " << fname.str() << " (" << columns << "x" << rows << ")";
      } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to save " << fname.str() << ": " << e.what() << "\n";
        return false;
      }
    }

    TLOG() << "  Group " << ig << ": " << kImagesPerSet << " images written\n";
  }

  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  try {
    // ---- Parse arguments ------------------------------------------------
    Args args;
    if (!parse_args(argc, argv, args)) {
      return 1;
    }

    // ---- Derived constants ----------------------------------------------
    uint16_t col_groups_per_image     = args.columns / kTicksPerPacket;             // cols/64
    uint32_t packets_per_image_group  = static_cast<uint32_t>(kPacketsPerGroup)
                                        * col_groups_per_image;                     // 20*(cols/64)

    // Round max_packets down to a multiple of packets_per_image_group
    if (args.max_packets % packets_per_image_group != 0) {
      args.max_packets = (args.max_packets / packets_per_image_group)
                         * packets_per_image_group;
      std::cout << "NOTE: --max-packets rounded down to " << args.max_packets
                << " (multiple of " << packets_per_image_group << ")\n";
    }

    // Validate min_packets is a multiple of packets_per_image_group
    if (args.min_packets % packets_per_image_group != 0) {
      uint32_t rounded = ((args.min_packets + packets_per_image_group - 1)
                          / packets_per_image_group) * packets_per_image_group;
      if (rounded > args.max_packets) {
        rounded = (args.max_packets / packets_per_image_group) * packets_per_image_group;
      }
      std::cout << "NOTE: --min-packets rounded to " << rounded
                << " (multiple of " << packets_per_image_group << ")\n";
      args.min_packets = rounded;
    }

    // Re-check after rounding
    if (args.min_packets > args.max_packets) {
      std::cerr << "ERROR: --min-packets (" << args.min_packets
                << ") exceeds --max-packets (" << args.max_packets
                << ") after rounding\n";
      return 1;
    }

    uint32_t max_image_groups     = args.max_packets / packets_per_image_group;
    uint32_t pixels_per_image_set = kRowsPerImageSet * args.columns;

    TLOG() << "Configuration:\n";
    TLOG() << "  max-packets:           " << args.max_packets << "\n";
    TLOG() << "  min-packets:           " << args.min_packets << "\n";
    TLOG() << "  columns:               " << args.columns << "\n";
    TLOG() << "  packets/image-group:   " << packets_per_image_group << "\n";
    TLOG() << "  max image groups:      " << max_image_groups << "\n";
    TLOG() << "  pixels/image-set:      " << pixels_per_image_set << "\n";
    TLOG() << "  max-png:               " << args.max_png << "\n";
    TLOG() << "  start-group:           " << args.start_group << "\n";
    TLOG() << "  mode:                  " << (args.use_gpu ? "GPU" : "CPU") << "\n";

    // ---- Allocate 5 memory blocks ---------------------------------------
    TLOG() << "Allocating memory blocks...\n";

    // Block 1: packet headers (74 B each)
    size_t block1_size = static_cast<size_t>(args.max_packets) * kHeaderBytesPerPkt;
    uint8_t* block1_headers = static_cast<uint8_t*>(std::malloc(block1_size));
    if (!block1_headers) {
      TLOG_ERROR() << "ERROR: Failed to allocate Block 1 (" << block1_size << " bytes)\n";
      return 1;
    }
    std::memset(block1_headers, 0, block1_size);
    TLOG() << "  Block 1 (headers):     " << block1_size << " bytes ("
              << (block1_size >> 20) << " MB)\n";

    // Block 2: header byte-offsets
    size_t block2_size = static_cast<size_t>(args.max_packets) * sizeof(size_t);
    size_t* block2_hdr_offsets = static_cast<size_t*>(std::malloc(block2_size));
    if (!block2_hdr_offsets) {
      TLOG_ERROR() << "ERROR: Failed to allocate Block 2\n";
      std::free(block1_headers);
      return 1;
    }
    std::memset(block2_hdr_offsets, 0, block2_size);
    TLOG() << "  Block 2 (hdr offsets): " << block2_size << " bytes\n";

    // Block 3: ADC byte-offsets
    size_t block3_size = static_cast<size_t>(args.max_packets) * sizeof(size_t);
    size_t* block3_adc_offsets = static_cast<size_t*>(std::malloc(block3_size));
    if (!block3_adc_offsets) {
      TLOG_ERROR() << "ERROR: Failed to allocate Block 3\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      return 1;
    }
    std::memset(block3_adc_offsets, 0, block3_size);
    TLOG() << "  Block 3 (ADC offsets): " << block3_size << " bytes\n";

    // Block 4: ADC data (7168 B each)
    size_t block4_size = static_cast<size_t>(args.max_packets) * kAdcBytesPerPacket;
    uint8_t* block4_adc = static_cast<uint8_t*>(std::malloc(block4_size));
    if (!block4_adc) {
      std::cerr << "ERROR: Failed to allocate Block 4 (" << block4_size << " bytes)\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets);
      return 1;
    }
    std::memset(block4_adc, 0, block4_size);
    TLOG() << "  Block 4 (ADC data):    " << block4_size << " bytes ("
              << (block4_size >> 20) << " MB)\n";

    // Block 5: image pixels (uint16_t, all groups)
    size_t block5_pixels = static_cast<size_t>(max_image_groups) * pixels_per_image_set;
    size_t block5_size   = block5_pixels * sizeof(uint16_t);
    uint16_t* block5_images = static_cast<uint16_t*>(std::malloc(block5_size));
    if (!block5_images) {
      TLOG_ERROR() << "ERROR: Failed to allocate Block 5 (" << block5_size << " bytes)\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      return 1;
    }
    std::memset(block5_images, 0, block5_size);
    TLOG() << "  Block 5 (images):      " << block5_size << " bytes ("
              << (block5_size >> 20) << " MB)\n";

    size_t total_alloc = block1_size + block2_size + block3_size
                         + block4_size + block5_size;
    TLOG() << "  Total allocated:       " << total_alloc << " bytes ("
              << (total_alloc >> 20) << " MB)\n\n";

    // ---- Read PCAP into Blocks 1-4 -------------------------------------
    TLOG() << "Reading PCAP file: " << args.input_file << "\n";

    PcapReader pcap(args.input_file);
    uint32_t total_packets = pcap.readPacketsBulk(
        block1_headers,  block2_hdr_offsets,
        block4_adc,      block3_adc_offsets,
        args.max_packets,
        kHeaderBytesPerPkt,
        kAdcBytesPerPacket,
        kNetHeaderSize);

    TLOG() << "Read " << total_packets << " packets from PCAP\n";

    if (total_packets == 0) {
      TLOG_ERROR() << "ERROR: No packets in PCAP file\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images);
      return 1;
    }

    // Trim to a whole number of image groups
    if (total_packets % packets_per_image_group != 0) {
      uint32_t usable = (total_packets / packets_per_image_group)
                        * packets_per_image_group;
      TLOG_NOTICE() << "NOTE: Using " << usable << " of " << total_packets
                << " packets (discarding " << (total_packets - usable)
                << " trailing packets)\n";
      total_packets = usable;
    }
    if (total_packets == 0) {
      TLOG_ERROR() << "ERROR: Not enough packets for one image group ("
                << packets_per_image_group << " needed)\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images);
      return 1;
    }

    // ---- Pad to min-packets if needed -----------------------------------
    if (total_packets < args.min_packets) {
      total_packets = pad_to_min_packets(
          block1_headers, block2_hdr_offsets,
          block4_adc, block3_adc_offsets,
          total_packets, args.min_packets, args.max_packets);
    }

    uint32_t num_image_groups = total_packets / packets_per_image_group;
    TLOG() << "Image groups: " << num_image_groups
              << " (of max " << max_image_groups << ")\n";

    // ---- Validate all groups of 20 -------------------------------------
    TLOG() << "Validating packet groups...\n";
    if (!validate_packets(block1_headers, block2_hdr_offsets,
                          total_packets, args.verbose)) {
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images);
      return 1;
    }

    // ---- Build lookup table ---------------------------------------------
    TLOG() << "Building lookup table...\n";
    int32_t* lookup = static_cast<int32_t*>(
        std::malloc(static_cast<size_t>(kLookupTotalInts) * sizeof(int32_t)));
    if (!lookup) {
      TLOG_ERROR() << "ERROR: Failed to allocate lookup table\n";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images);
      return 1;
    }

    if (!build_lookup_table(lookup, args.plugin)) {
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images); std::free(lookup);
      return 1;
    }

    // ---- Scatter ADC data into image block ------------------------------
    TLOG() << "Scattering ADC data to images ("
              << (args.use_gpu ? "GPU" : "CPU") << ")...\n";

    if (args.use_gpu) {
#ifdef HAVE_ICEBERG_GPU
      scatter_adc_to_images_gpu(
          block4_adc, block3_adc_offsets, lookup, block5_images,
          packets_per_image_group, num_image_groups,
          args.columns, pixels_per_image_set);
      TLOG() << "Scatter via gpu complete.\n";
#else
      TLOG_ERROR() << "ERROR: --gpu requested but this build does not have GPU support.\n"
                   << "  Rebuild with ICEBERG_GPU environment variable set and CUDAToolkit installed.\n"
                   << "  (i.e: ICEBERG_GPU= dbt-build -c)";
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images); std::free(lookup);
      return 1;
#endif
    } else {
      scatter_adc_to_images_cpu(
          block4_adc, block3_adc_offsets, lookup, block5_images,
          packets_per_image_group, num_image_groups,
          args.columns, pixels_per_image_set);
      TLOG() << "Scatter via cpu complete.\n";
    }

    // ---- Optionally (via TLOG_DEBUG_SCOPED) display first 16 pixels of first 4 rows of first image for debugging ---------------
    TLOG_DEBUG_SCOPED(10) {
      uint64_t* as_uint64 = reinterpret_cast<uint64_t*>(block5_images); // for 4 16-bit pixels per 64-bit word
      TLOG_ADD       << "image0 row0: " << std::setfill('0') << std::setw(16) << std::hex << as_uint64[  0] << " " << as_uint64[  1] << " " << as_uint64[  2] << " " << as_uint64[  3];
      TLOG_DEBUG(13) << "image0 row3: " << std::setfill('0') << std::setw(16) << std::hex << as_uint64[192] << " " << as_uint64[193] << " " << as_uint64[194] << " " << as_uint64[195];
      TLOG_DEBUG(12) << "image0 row2: " << std::setfill('0') << std::setw(16) << std::hex << as_uint64[128] << " " << as_uint64[129] << " " << as_uint64[130] << " " << as_uint64[131];
      TLOG_DEBUG(11) << "image0 row1: " << std::setfill('0') << std::setw(16) << std::hex << as_uint64[ 64] << " " << as_uint64[ 65] << " " << as_uint64[ 66] << " " << as_uint64[ 67];
    }
    // ---- Write PNG images -----------------------------------------------
    if (!write_png_images(block5_images, num_image_groups,
                          args.columns, pixels_per_image_set,
                          args.start_group, args.max_png,
                          args.output_dir, args.output_prefix)) {
      std::free(block1_headers); std::free(block2_hdr_offsets);
      std::free(block3_adc_offsets); std::free(block4_adc);
      std::free(block5_images); std::free(lookup);
      return 1;
    }

    // ---- Cleanup --------------------------------------------------------
    std::free(lookup);
    std::free(block1_headers);
    std::free(block2_hdr_offsets);
    std::free(block3_adc_offsets);
    std::free(block4_adc);
    std::free(block5_images);

    TLOG_INFO() << "Success!\n";
    return 0;

  } catch (const std::exception& e) {
    TLOG_ERROR() << "Fatal error: " << e.what() << "\n";
    return 1;
  }
}
