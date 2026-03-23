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

int main(int argc, char* argv[]) {
  int packet_limit = -1;
  bool print_delta = false;
  int delta_width = 5;
  const char* pcap_file = nullptr;

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
    } else if (!arg.empty() && arg.front() == '-') {
      std::cerr << "Usage: " << argv[0] << " [-n <packet_count>] [--delta[=<column_width>]] <input.pcap>" << std::endl;
      return 1;
    } else if (pcap_file == nullptr) {
      pcap_file = argv[argi];
    } else {
      std::cerr << "Usage: " << argv[0] << " [-n <packet_count>] [--delta[=<column_width>]] <input.pcap>" << std::endl;
      return 1;
    }
  }

  if (pcap_file == nullptr) {
    std::cerr << "Usage: " << argv[0] << " [-n <packet_count>] [--delta[=<column_width>]] <input.pcap>" << std::endl;
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

    previous_timestamp = timestamp;
    have_previous_timestamp = true;
  }

  return 0;
}
