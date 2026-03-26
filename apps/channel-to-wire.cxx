/**
 * @file channel-to-wire.cxx
 *
 * Developer(s) of this DAQ application have yet to replace this line with a brief description of the application.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include <exception>
#include <string>

#include "TRACE/trace.h"

#include "IcebergWireChannelMap.hpp"

using geo::IcebergWireChannelMap;
using geo::WireID;




int
main(int argc, char** argv)
{
  try {
    TRACE(TLVL_DEBUG+1, "hello");

    auto printHelp = [argv]() {
      printf("Usage:\n");
      printf("  %s [--one-line] <offline_chan>\n", basename(argv[0]));
      printf("  %s [--one-line] --plane=<plane> --tpc=<tpc> --wire=<wire>\n", basename(argv[0]));
      printf("\n");
      printf("Options:\n");
      printf("  --one-line   Print one output line (forward mode only).\n");
      printf("  --plane=     Plane index for reverse lookup.\n");
      printf("  --tpc=       TPC index for reverse lookup.\n");
      printf("  --wire=      Wire index for reverse lookup.\n");
      printf("  -h, --help   Show this help text.\n");
      printf("\n");
      printf("Notes:\n");
      printf("  If any of --plane=, --tpc=, or --wire= is given, all three are required.\n");
      printf("  In reverse mode, <offline_chan> must not be provided.\n");
      printf("  Reverse mode uses cryostat 0 when constructing WireID.\n");
    };

    bool oneLineMode = false;
    int chanArgIndex = -1;
    int nPositionalArgs = 0;
    bool havePlane = false;
    bool haveTpc = false;
    bool haveWire = false;
    unsigned plane = 0;
    unsigned tpc = 0;
    unsigned wire = 0;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-h" || arg == "--help") {
        printHelp();
        return 0;
      } else if (arg == "--one-line") {
        oneLineMode = true;
      } else if (arg.rfind("--plane=", 0) == 0) {
        havePlane = true;
        plane = strtoul(arg.c_str() + 8, NULL, 0);
      } else if (arg.rfind("--tpc=", 0) == 0) {
        haveTpc = true;
        tpc = strtoul(arg.c_str() + 6, NULL, 0);
      } else if (arg.rfind("--wire=", 0) == 0) {
        haveWire = true;
        wire = strtoul(arg.c_str() + 7, NULL, 0);
      } else if (arg[0] != '-') {
        chanArgIndex = i;
        ++nPositionalArgs;
      } else {
        TLOG_ERROR()<<"Unknown option: "<<arg;
        return 1;
      }
    }

    bool reverseMode = havePlane || haveTpc || haveWire;
    if (reverseMode && !(havePlane && haveTpc && haveWire)) {
      TLOG_ERROR()<<"If any of --plane=, --tpc=, or --wire= is provided, all three must be provided.";
      printHelp();
      return 1;
    }

    if (reverseMode && nPositionalArgs > 0) {
      TLOG_ERROR()<<"Do not provide <offline_chan> when using --plane=, --tpc=, and --wire=.";
      return 1;
    }

    if (!reverseMode && nPositionalArgs != 1) {
      printHelp();
      return 1;
    }

    IcebergWireChannelMap readout{};

    if (reverseMode) {
      WireID wid(0, tpc, plane, wire);
      unsigned chan = readout.PlaneWireToChannel(wid);
      TLOG()<<"off_chan: "<<chan
            <<" tpc="<<tpc<<" plane="<<plane<<" wire: "<<wire;
      return 0;
    }

    unsigned chan = strtoul(argv[chanArgIndex], NULL, 0);

    std::string sview = "UVZ";
    auto wids = readout.ChannelToWire(chan);

    if (oneLineMode) {
      if (wids.size() == 1) {
        TLOG()<<"off_chan: "<<std::setw(4)<<chan<<std::setw(0)
              <<" tpc="<<wids[0].TPC<<" plane="<<sview[wids[0].Plane]<<" wire: "<<std::setw(3)<<wids[0].Wire
              <<" image: "<<std::setw(0)<<(wids[0].TPC + wids[0].Plane*2);
      }
      if (wids.size() > 1) {
        TLOG()<<"off_chan:"<<std::setw(4)<<chan<<std::setw(0)
              <<" tpc="<<wids[0].TPC<<" plane="<<sview[wids[0].Plane]<<" wire: "<<std::setw(3)<<wids[0].Wire
              <<" and tpc="<<wids[1].TPC<<" plane="<<sview[wids[1].Plane]<<" wire: "<<std::setw(3)<<wids[1].Wire
              <<" images: "<<std::setw(0)<<(wids[0].TPC + wids[0].Plane*2)<<","<<std::setw(0)<<(wids[1].TPC + wids[1].Plane*2);
      }
    } else {
      for (size_t i = 0; i < wids.size(); ++i) {
        TLOG()<<"off_chan: "<<std::setw(4)<<chan<<std::setw(0)
              <<" tpc="<<wids[i].TPC<<" plane="<<sview[wids[i].Plane]<<" wire: "<<std::setw(3)<<wids[i].Wire
              <<" image: "<<std::setw(0)<<(wids[i].TPC + wids[i].Plane*2);
      }
    }

    return 0;
  } catch (std::exception const& ex) {
    TLOG_ERROR() << ex.what();
    return 1;
  }
}
