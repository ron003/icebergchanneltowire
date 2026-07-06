#ifndef PACKET_ORDER_MAP_HPP
#define PACKET_ORDER_MAP_HPP

#include "detchannelmaps/TPCChannelMap.hpp"
#include "TRACE/trace.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

static constexpr int kPacketOrderMapSize = 20;

struct PacketPattern {
  uint16_t crate;
  uint16_t slot;
  uint16_t stream;
};

struct PacketOrderMap {
  std::vector<PacketPattern> order;
  std::unordered_map<uint32_t, unsigned int> reverse;
};

inline uint32_t encodeSlotStreamIdx(uint16_t slot, unsigned int stream_idx) {
  return (static_cast<uint32_t>(slot) << 16) | stream_idx;
}

// Derive the expected packet order by iterating all offline channels through
// the channel map.  Returns the forward table (packet_index → crate/slot/stream)
// and the reverse map ((slot, stream_idx) → packet_index).
inline PacketOrderMap
buildPacketOrderMap(dunedaq::detchannelmaps::TPCChannelMap& chan_map,
                    uint32_t total_channels = 1280) {
  // Use a set of (slot, stream) to collect unique packets in sorted order.
  // Encoding: slot in upper 16 bits, stream in lower 16 bits — gives
  // the natural (slot ascending, stream ascending) sort.
  struct CrateSlotStream {
    uint16_t crate;
    uint16_t slot;
    uint16_t stream;
    bool operator<(const CrateSlotStream& o) const {
      if (slot != o.slot) return slot < o.slot;
      return stream < o.stream;
    }
    bool operator==(const CrateSlotStream& o) const {
      return crate == o.crate && slot == o.slot && stream == o.stream;
    }
  };

  std::set<CrateSlotStream> unique_packets;

  for (uint32_t off_chan = 0; off_chan < total_channels; ++off_chan) {
    auto coords = chan_map.get_crate_slot_fiber_chan_from_offline_channel(off_chan);
    if (!coords.has_value()) continue;

    uint16_t stream = static_cast<uint16_t>(
        ((coords->fiber & 0x1U) << 6) | ((coords->channel / 64) & 0x3U));

    unique_packets.insert({static_cast<uint16_t>(coords->crate),
                           static_cast<uint16_t>(coords->slot),
                           stream});
  }

  if (unique_packets.size() != static_cast<size_t>(kPacketOrderMapSize)) {
    throw std::runtime_error(
        "Expected " + std::to_string(kPacketOrderMapSize) +
        " unique packets but found " + std::to_string(unique_packets.size()));
  }

  {
    std::ostringstream oss;
    oss << "\nDerived packet order (" << unique_packets.size() << " packets):\n"
        << "  Pkt  crate  slot  stream\n"
        << "  ---  -----  ----  ------\n";
    int i = 0;
    for (const auto& p : unique_packets) {
      oss << "\n  " << std::setw(3) << i
          << "  " << std::setw(5) << p.crate
          << "  " << std::setw(4) << p.slot
          << "  " << std::setw(6) << p.stream;
      ++i;
    }
    TLOG() << oss.str();
  }

  PacketOrderMap result;
  result.order.reserve(kPacketOrderMapSize);

  unsigned int idx = 0;
  for (const auto& p : unique_packets) {
    result.order.push_back({p.crate, p.slot, p.stream});

    unsigned int fiber     = (p.stream >> 6) & 1;
    unsigned int locstream = p.stream & 0x3;
    unsigned int stream_idx = locstream + (fiber << 2);
    result.reverse[encodeSlotStreamIdx(p.slot, stream_idx)] = idx;
    ++idx;
  }

  return result;
}

#endif // PACKET_ORDER_MAP_HPP
