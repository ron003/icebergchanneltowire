#include <stdio.h>
#include <stdint.h>                             // uint32_t
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <net/ethernet.h>               // for struct ether_header
#include <netinet/ip.h>                 // struct ip
#include <netinet/udp.h>                // struct udphdr
#include <arpa/inet.h>                  // ntohs, ntohl
#include <string.h>                             // memcpy, etc.
#include "TRACE/trace.h"
#include "fddetdataformats/WIBEthFrame.hpp" // nests detdataformats::DAQEthHeader

struct pcap_pkthdr {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t caplen;
  uint32_t len;
};

struct pcap_file_header {
  uint32_t magic_number;
  uint16_t version_major;
  uint16_t version_minor;
  int32_t  thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;
};

// Generate a test PCAP file with one packet containing a WIBEthFrame
// with incrementing ADC values: adc[channel][tick] = ((channel & 0x3f) << 8) | (tick & 0x3f)
static int generate_test_pcap(const char* filename) {
  using dunedaq::fddetdataformats::WIBEthFrame;

  // Build the complete packet: Ethernet + IP + UDP + WIBEthFrame
  constexpr size_t eth_hdr_size = sizeof(struct ether_header);
  constexpr size_t ip_hdr_size = sizeof(struct ip);
  constexpr size_t udp_hdr_size = sizeof(struct udphdr);
  constexpr size_t wib_frame_size = sizeof(WIBEthFrame);
  constexpr size_t total_packet_size = eth_hdr_size + ip_hdr_size + udp_hdr_size + wib_frame_size;

  std::vector<uint8_t> packet(total_packet_size, 0);

  // Ethernet header
  auto* eth = reinterpret_cast<struct ether_header*>(packet.data());
  eth->ether_type = htons(ETHERTYPE_IP);

  // IP header
  auto* ip_hdr = reinterpret_cast<struct ip*>(packet.data() + eth_hdr_size);
  ip_hdr->ip_hl = 5;
  ip_hdr->ip_v = 4;
  ip_hdr->ip_len = htons(static_cast<uint16_t>(ip_hdr_size + udp_hdr_size + wib_frame_size));
  ip_hdr->ip_p = IPPROTO_UDP;

  // UDP header
  auto* udp_hdr = reinterpret_cast<struct udphdr*>(packet.data() + eth_hdr_size + ip_hdr_size);
  udp_hdr->uh_ulen = htons(static_cast<uint16_t>(udp_hdr_size + wib_frame_size));

  // WIBEthFrame - fill with incrementing pattern
  auto* wib_frame = reinterpret_cast<WIBEthFrame*>(packet.data() + eth_hdr_size + ip_hdr_size + udp_hdr_size);

  // Initialize the DAQ header
  wib_frame->daq_header.crate_id = 8;
  wib_frame->daq_header.slot_id = 2;
  wib_frame->daq_header.stream_id = 0;

  // Fill ADC values: adc[channel][tick] = ((channel & 0x3f) << 8) | (tick & 0x3f)
  for (int ch = 0; ch < static_cast<int>(WIBEthFrame::s_num_channels); ++ch) {
    for (int tick = 0; tick < static_cast<int>(WIBEthFrame::s_time_samples_per_frame); ++tick) {
      uint16_t adc_value = static_cast<uint16_t>(((ch & 0x3f) << 8) | (tick & 0x3f));
      wib_frame->set_adc(ch, tick, adc_value);
    }
  }

  // Write PCAP file
  std::ofstream outfile(filename, std::ios::binary);
  if (!outfile) {
    std::cerr << "ERROR: Could not create file " << filename << std::endl;
    return 1;
  }

  // Global header
  struct pcap_file_header global_hdr = {};
  global_hdr.magic_number = 0xa1b2c3d4;
  global_hdr.version_major = 2;
  global_hdr.version_minor = 4;
  global_hdr.snaplen = 65535;
  global_hdr.network = 1; // Ethernet
  outfile.write(reinterpret_cast<const char*>(&global_hdr), sizeof(global_hdr));

  // Packet header
  struct pcap_pkthdr pkt_hdr = {};
  pkt_hdr.caplen = static_cast<uint32_t>(total_packet_size);
  pkt_hdr.len = static_cast<uint32_t>(total_packet_size);
  outfile.write(reinterpret_cast<const char*>(&pkt_hdr), sizeof(pkt_hdr));

  // Packet data
  outfile.write(reinterpret_cast<const char*>(packet.data()), static_cast<std::streamsize>(total_packet_size));

  std::cout << "Generated test PCAP: " << filename << std::endl;
  std::cout << "  1 packet, " << total_packet_size << " bytes" << std::endl;
  std::cout << "  WIBEthFrame: " << WIBEthFrame::s_num_channels << " channels x "
            << WIBEthFrame::s_time_samples_per_frame << " ticks" << std::endl;
  std::cout << "  ADC pattern: adc[ch][tick] = ((ch & 0x3f) << 8) | (tick & 0x3f)" << std::endl;
  std::cout << "  Example: ch0 tick0-3 = 0000 0001 0002 0003" << std::endl;
  std::cout << "           ch1 tick0-3 = 0100 0101 0102 0103" << std::endl;

  return 0;
}

int main(int argc, char* argv[]) {
  int packet_limit = -1;
  bool print_delta = false;
  int delta_width = 5;
  const char* pcap_file = nullptr;
  const char* generate_file = nullptr;

  // --data option: pkt (1-based), pktchn (0-63), tickcnt
  bool print_data = false;
  int data_pkt = 0;
  int data_pktchn = 0;
  int data_tickcnt = 0;

  for (int argi = 1; argi < argc; ++argi) {
    const std::string arg = argv[argi];

    if (arg == "-n") {
      if (argi + 1 >= argc) {
        std::cerr << "ERROR: -n requires a packet count" << std::endl;
        return 1;
      }
      packet_limit = std::stoi(argv[++argi]);
      if (packet_limit < 0) {
        std::cerr << "ERROR: -n must be >= 0" << std::endl;
        return 1;
      }
    } else if (arg == "--delta" || arg.rfind("--delta=", 0) == 0) {
      print_delta = true;
      if (arg == "--delta") {
        if (argi + 1 < argc) {
          const std::string next_arg = argv[argi + 1];
          if (!next_arg.empty() && next_arg.front() != '-') {
            delta_width = std::stoi(next_arg);
            ++argi;
          }
        }
      } else {
        delta_width = std::stoi(arg.substr(std::string("--delta=").size()));
      }

      if (delta_width < 0) {
        std::cerr << "ERROR: --delta width must be >= 0" << std::endl;
        return 1;
      }
    } else if (arg.rfind("--data=", 0) == 0) {
      std::string data_arg = arg.substr(std::string("--data=").size());
      // Parse comma-separated values: pkt,pktchn,tickcnt
      size_t pos1 = data_arg.find(',');
      size_t pos2 = (pos1 != std::string::npos) ? data_arg.find(',', pos1 + 1) : std::string::npos;
      if (pos1 == std::string::npos || pos2 == std::string::npos) {
        std::cerr << "ERROR: --data requires 3 comma-separated values: --data=<pkt,pktchn,tickcnt>" << std::endl;
        return 1;
      }
      try {
        data_pkt = std::stoi(data_arg.substr(0, pos1));
        data_pktchn = std::stoi(data_arg.substr(pos1 + 1, pos2 - pos1 - 1));
        data_tickcnt = std::stoi(data_arg.substr(pos2 + 1));
      } catch (const std::exception& e) {
        std::cerr << "ERROR: --data requires 3 numeric values: --data=<pkt,pktchn,tickcnt>" << std::endl;
        return 1;
      }
      if (data_pkt < 1) {
        std::cerr << "ERROR: --data pkt must be >= 1 (1-based packet number)" << std::endl;
        return 1;
      }
      if (data_pktchn < 0 || data_pktchn > 63) {
        std::cerr << "ERROR: --data pktchn must be 0-63" << std::endl;
        return 1;
      }
      if (data_tickcnt < 1) {
        std::cerr << "ERROR: --data tickcnt must be >= 1" << std::endl;
        return 1;
      }
      print_data = true;
    } else if (arg.rfind("--generate=", 0) == 0) {
      generate_file = argv[argi] + std::string("--generate=").size();
    } else if (!arg.empty() && arg.front() == '-') {
      std::cerr << "Usage: " << argv[0] << " [--generate=<output.pcap>] | [-n <packet_count>] [--delta[=<column_width>]] [--data=<pkt,pktchn,tickcnt>] <input.pcap>" << std::endl;
      return 1;
    } else if (pcap_file == nullptr) {
      pcap_file = argv[argi];
    } else {
      std::cerr << "Usage: " << argv[0] << " [--generate=<output.pcap>] | [-n <packet_count>] [--delta[=<column_width>]] [--data=<pkt,pktchn,tickcnt>] <input.pcap>" << std::endl;
      return 1;
    }
  }

  // Handle --generate mode
  if (generate_file != nullptr) {
    return generate_test_pcap(generate_file);
  }

  if (pcap_file == nullptr) {
    std::cerr << "Usage: " << argv[0] << " [--generate=<output.pcap>] | [-n <packet_count>] [--delta[=<column_width>]] [--data=<pkt,pktchn,tickcnt>] <input.pcap>" << std::endl;
    return 1;
  }

  std::ifstream infile(pcap_file, std::ios::binary);
  if (!infile) {
    std::cerr << "ERROR: Could not open file " << pcap_file << std::endl;
    return 1;
  }

  // Skip the global header (24 bytes)
  infile.seekg(24);

  std::cout << "Pkt timestamp        crate slot stream ";
  if (print_delta) {
    std::cout << std::left << std::setw(delta_width) << "delta" << std::right << ' ';
  }
  std::cout << "UDPbyts" << std::endl;

  std::cout << "--- ---------------- ----- ---- ------ ";
  if (print_delta) {
    std::cout << std::string(static_cast<size_t>(delta_width), '-') << ' ';
  }
  std::cout << "-------" << std::endl;

  int packet_count = 0;
  uint64_t previous_timestamp = 0;
  bool have_previous_timestamp = false;
  while (infile) {
    if (packet_limit >= 0 && packet_count >= packet_limit) {
      break;
    }

    // Read packet header (16 bytes)
    struct pcap_pkthdr pkt_header;
    infile.read(reinterpret_cast<char*>(&pkt_header), sizeof(pkt_header));
    if (infile.gcount() != sizeof(pkt_header)) {
      break; // EOF or error
    }

    // Read packet data
    std::vector<uint8_t> packet_data(pkt_header.caplen);
    infile.read(reinterpret_cast<char*>(packet_data.data()), pkt_header.caplen);
    if (infile.gcount() != pkt_header.caplen) {
      break; // EOF or error
    }

    ++packet_count;

    if (pkt_header.caplen < sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr)) {
      continue;
    }

    const uint8_t* data = packet_data.data();
    const struct ether_header* eth = reinterpret_cast<const struct ether_header*>(data);
    if (ntohs(eth->ether_type) != ETHERTYPE_IP) {
      continue;
    }

    const size_t ip_offset = sizeof(struct ether_header);
    const struct ip* ip_header = reinterpret_cast<const struct ip*>(data + ip_offset);
    if (ip_header->ip_p != IPPROTO_UDP) {
      continue;
    }

    const size_t ip_header_size = static_cast<size_t>(ip_header->ip_hl) * 4;
    if (ip_header_size < sizeof(struct ip) ||
        pkt_header.caplen < ip_offset + ip_header_size + sizeof(struct udphdr)) {
      continue;
    }

    const struct udphdr* udp_header = reinterpret_cast<const struct udphdr*>(data + ip_offset + ip_header_size);
    const uint16_t udp_len = ntohs(udp_header->uh_ulen);
    if (udp_len < sizeof(struct udphdr)) {
      continue;
    }

    size_t udp_payload_size = udp_len - sizeof(struct udphdr);
    const size_t udp_payload_offset = ip_offset + ip_header_size + sizeof(struct udphdr);
    const size_t captured_udp_payload_size = (pkt_header.caplen > udp_payload_offset) ?
      (pkt_header.caplen - udp_payload_offset) : 0;
    if (udp_payload_size > captured_udp_payload_size) {
      udp_payload_size = captured_udp_payload_size;
    }

    if (udp_payload_size < sizeof(dunedaq::detdataformats::DAQEthHeader)) {
      continue;
    }

    const auto* daq_header = reinterpret_cast<const dunedaq::detdataformats::DAQEthHeader*>(data + udp_payload_offset);
    const uint64_t timestamp = daq_header->get_timestamp();
    const unsigned crate_id = daq_header->crate_id;
    const unsigned slot_id  = daq_header->slot_id;
    const unsigned stream_id= daq_header->stream_id;

    std::ostringstream output;
    output << std::setw(3) << std::setfill(' ') << packet_count
           << ' '
           << std::hex << std::setw(16) << std::setfill('0') << timestamp
           << std::dec;

    // print crate/slot/stream columns (reset fill to space)
    output << ' ' << std::setw(5) << std::setfill(' ') << crate_id
           << ' ' << std::setw(4) << std::setfill(' ') << slot_id
           << ' ' << std::setw(6) << std::setfill(' ') << stream_id;

    if (print_delta) {
      output << ' ';
      if (have_previous_timestamp) {
        output << std::setw(delta_width) << std::setfill(' ') << (timestamp - previous_timestamp);
      } else {
        output << std::setw(delta_width) << std::setfill(' ') << "-";
      }
    }

    output << ' ' << std::setw(7) << std::setfill(' ') << udp_payload_size;
    std::cout << output.str() << std::endl;

    // Print ADC data if --data option matches this packet
    if (print_data && packet_count == data_pkt) {
      if (udp_payload_size >= sizeof(dunedaq::fddetdataformats::WIBEthFrame)) {
        const auto* wib_frame = reinterpret_cast<const dunedaq::fddetdataformats::WIBEthFrame*>(data + udp_payload_offset);
        std::cout << "    ADC[ch" << data_pktchn << "]:";
        int max_ticks = std::min(data_tickcnt, static_cast<int>(dunedaq::fddetdataformats::WIBEthFrame::s_time_samples_per_frame));
        for (int tick = 0; tick < max_ticks; ++tick) {
          std::cout << ' ' << std::hex << std::setw(4) << std::setfill('0') << wib_frame->get_adc(data_pktchn, tick);
        }
        std::cout << std::dec << std::endl;
      } else {
        std::cout << "    (packet too small for WIBEthFrame ADC data)" << std::endl;
      }
    }

    previous_timestamp = timestamp;
    have_previous_timestamp = true;
  }

  return 0;
}
