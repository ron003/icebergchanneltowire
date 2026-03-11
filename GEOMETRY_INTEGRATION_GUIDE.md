# Geometry Integration Guide

## Overview

The `images-to-pcap` application is currently built with stub implementations for wire-to-channel conversion and PCAP packet generation. This guide shows how to integrate the real detector geometry.

## Current Stub Code

In `images-to-pcap.cxx`, the `generatePcap()` function contains placeholder code:

```cpp
// For each group of 64 columns
uint16_t num_column_groups = common_columns_ / 64;

for (uint16_t col_group = 0; col_group < num_column_groups; ++col_group) {
  // Create a simple test packet for each column group
  uint32_t packet_size = 1024;
  std::vector<uint8_t> packet_data(packet_size);
  memset(packet_data.data(), 0xAA, packet_size);
  pcap.writePacket(packet_data.data(), packet_data.size());
}
```

## Step 1: Update CMakeLists.txt

Add geometry package dependencies:

```cmake
find_package(larcorealg REQUIRED)      # For DuneApaWireReadoutGeom
find_package(detchannelmaps REQUIRED)  # For TPCChannelMap
```

Update the `images-to-pcap` application build:

```cmake
daq_add_application(images-to-pcap 
    images-to-pcap.cxx 
    PngImageLoader.cpp 
    PcapWriter.cpp 
    LINK_LIBRARIES 
        TRACE::TRACE 
        PNG::PNG
        larcorealg::larcorealg
        detchannelmaps::detchannelmaps)
```

## Step 2: Update includes-to-pcap.cxx

Replace the stub `WireID` and geometry references:

### Remove
```cpp
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
```

### Add
```cpp
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"      // geo::WireID
#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h"       // raw::ChannelID_t
#include "larcorealg/Geometry/DuneApaWireReadoutGeom.h"     // DuneApaWireReadoutGeom
#include "detchannelmaps/TPCChannelMap.hpp"                 // TPCChannelMap
#include "detchannelmaps/Loader.hpp"                        // Channel map loader
```

## Step 3: Initialize Geometry Objects

Add to `ImagesTopcap` class:

```cpp
class ImagesTopcap {
private:
  std::unique_ptr<geo::DuneApaWireReadoutGeom> wire_geom_;
  std::shared_ptr<dunedaq::detchannelmaps::TPCChannelMap> channel_map_;
  // ... existing members
};
```

Initialize in constructor or `processImages()`:

```cpp
// Load channel map
try {
  dunedaq::detchannelmaps::ChannelMapLoader loader;
  channel_map_ = loader.load();  // Actual loading implementation
} catch (const std::exception& e) {
  std::cerr << "ERROR: Failed to load channel map: " << e.what() << std::endl;
  return false;
}

// Initialize geometry (constructor may load from configuration)
wire_geom_ = std::make_unique<geo::DuneApaWireReadoutGeom>();
```

## Step 4: Implement Real Packet Generation

Replace the stub `generatePcap()` function body:

```cpp
bool ImagesTopcap::generatePcap() {
  std::cout << "Generating PCAP file: " << args_.output_file << std::endl;

  try {
    PcapWriter pcap(args_.output_file);

    uint16_t num_column_groups = common_columns_ / 64;
    
    std::cout << "Processing " << num_column_groups << " column groups..." << std::endl;

    // For each group of 64 columns
    for (uint16_t col_group = 0; col_group < num_column_groups; ++col_group) {
      
      // For each plane
      for (const auto& [plane_key, plane_info] : planes_) {
        
        // For each row (wire)
        for (uint16_t row = 0; row < plane_info.data.height; ++row) {
          uint16_t wire = row;
          
          // Get pixel value from image
          uint16_t pixel_value = plane_info.data.pixels[row * plane_info.data.width + 
                                                        col_group * 64];
          
          // Create WireID: (cryostat=0, tpc, plane_index, wire)
          uint32_t plane_idx = getPlaneIndex(plane_info.plane_char);
          geo::WireID wireid(0, plane_info.tpc_num, plane_idx, wire);
          
          // Get offline channel from geometry
          raw::ChannelID_t offline_channel = 
              wire_geom_->PlaneWireToChannel(wireid);
          
          if (offline_channel == raw::ChannelID_t(-1)) {
            TLOG_WARN(TRACE_NAME) << "Invalid wire-to-channel mapping for "
                                  << "wire " << wire;
            continue;
          }
          
          // Get crate/slot/fiber from channel map
          auto tpc_coords = 
              channel_map_->get_crate_slot_fiber_chan_from_offline_channel(
                  offline_channel);
          
          if (!tpc_coords) {
            TLOG_WARN(TRACE_NAME) << "No channel map entry for offline channel "
                                  << offline_channel;
            continue;
          }
          
          // Build UDP/WIB packet
          std::vector<uint8_t> packet_data = buildWibPacket(
              tpc_coords->crate,
              tpc_coords->slot,
              tpc_coords->fiber,
              tpc_coords->channel,
              pixel_value,
              wireID);
          
          // Write to PCAP
          pcap.writePacket(packet_data.data(), packet_data.size());
        }
      }
    }

    std::cout << "PCAP file generated successfully" << std::endl;
    return true;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to generate PCAP: " << e.what() << std::endl;
    return false;
  }
}
```

## Step 5: Implement WIB Packet Builder

Add helper function to construct proper WIB frames:

```cpp
std::vector<uint8_t> ImagesTopcap::buildWibPacket(
    uint32_t crate,
    uint32_t slot,
    uint32_t fiber,
    uint32_t channel,
    uint16_t adc_value,
    const geo::WireID& wireid) {
  
  // This is pseudocode - actual WIB frame format varies
  std::vector<uint8_t> packet;
  
  // Ethernet header (14 bytes)
  packet.resize(14 + 20 + 8 + 256);  // ETH + IP + UDP + payload
  
  // Ethernet frame
  // (MAC dest, MAC src, EtherType)
  
  // IP header
  // (src IP, dst IP, protocol=UDP)
  
  // UDP header
  // (src port, dst port, length)
  
  // WIB payload
  // (crate, slot, fiber, channel, adc_value, etc.)
  
  return packet;
}
```

## Building Blocks Available

### From `larcorealg`
```cpp
geo::WireID wireid(cryostat, tpc, plane, wire);
raw::ChannelID_t channel = geom.PlaneWireToChannel(wireid);
std::vector<WireID> wires = geom.ChannelToWire(channel);
```

### From `detchannelmaps`
```cpp
// Get detector element coordinates from channel
std::optional<TPCChannelMap::TPCCoords> coords = 
    channel_map->get_crate_slot_fiber_chan_from_offline_channel(channel);
// Result: coords->crate, coords->slot, coords->fiber, coords->channel
```

### From `PngImageLoader`
```cpp
// Load image
auto image = PngImageLoader::loadImage("u0.png");
// Access: image.width, image.height, image.pixels[row * width + col]
```

### From `PcapWriter`
```cpp
// Write packet with timestamp
pcap.writePacket(data.data(), data.size(), timestamp_us);
```

## Testing Checklist

After integration, verify:

- [ ] Application compiles without errors
- [ ] Can load geometry and channel maps
- [ ] Wire-to-channel conversion produces expected values
- [ ] Channel-to-detector conversion produces crate/slot/fiber
- [ ] Generated PCAP file is valid (can open in Wireshark)
- [ ] Packet count matches expected (planes × height × column_groups)
- [ ] Packet timestamps are properly sequenced
- [ ] ADC values from images appear in packet payload

## Validation

Compare generated PCAP with sample PCAP file:

```bash
# Extract packet count
tcpdump -r reference.pcap | wc -l
tcpdump -r generated.pcap | wc -l

# Compare packet structure
tcpdump -r reference.pcap -x | head -20
tcpdump -r generated.pcap -x | head -20

# Full dump for analysis
tcpdump -r generated.pcap -vvv > generated_packets.txt
```

## Error Handling

Ensure robust error handling:

- [ ] Invalid WireID handling
- [ ] Missing channel map entries
- [ ] Out-of-range channel numbers
- [ ] Geometry initialization failures
- [ ] Timestamp overflow scenarios
- [ ] Large file I/O errors

## Performance Optimization

For production use, consider:

1. **Batch Processing**: Process multiple planes in parallel
2. **Memory Pool**: Pre-allocate packet buffers
3. **Caching**: Cache frequently used geometry lookups
4. **Streaming**: Don't load entire image if processing row-by-row
5. **Compression**: Could add PCAP file compression

## References

- PCAP Format: https://www.tcpdump.org/papers/sniffing-faq.html#Q1
- LArCore Geometry: Use local documentation in `larcorealg`
- WIB Format: Refer to DUNE-DAQ hardware documentation
- PNG Format: http://www.libpng.org/pub/png/spec/
