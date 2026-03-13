#include "IcebergWireChannelMap.hpp"

#include <array>
#include <sstream>
#include <stdexcept>

#include "TRACE/trace.h"

namespace {

using Index = geo::IcebergWireChannelMap::Index;

constexpr std::array<unsigned int, 3> kWiresPerPlane = {316, 315, 240};
constexpr std::array<geo::View_t, 3> kPlaneViews = {geo::kU, geo::kV, geo::kZ};

std::out_of_range make_out_of_range(std::string const& message)
{
  return std::out_of_range(message);
}

std::runtime_error make_runtime_error(std::string const& message)
{
  return std::runtime_error(message);
}

std::string format_wire_id(geo::WireID const& wire_id)
{
  std::ostringstream ss;
  ss << "cryostat=" << wire_id.Cryostat << " tpc=" << wire_id.TPC << " plane=" << wire_id.Plane
     << " wire=" << wire_id.Wire;
  return ss.str();
}

} // namespace

namespace geo {

IcebergWireChannelMap::IcebergWireChannelMap()
{
  Index const ncry = 1;
  Index const ntpc = 2;
  Index const npla = kWiresPerPlane.size();

  fNchannels = 1280;
  fNcryostat = ncry;
  fNApa.resize(ncry);
  fWiresPerPlane.resize(ncry);
  fPlaneApa.resize(ncry);
  fPlaneRop.resize(ncry);
  fPlaneRopIndex.resize(ncry);
  fAnchoredWires.resize(ncry);
  fFirstChannelInThisRop.resize(ncry);
  fFirstChannelInNextRop.resize(ncry);
  fRopsPerApa.resize(ncry);
  fPlanesPerRop.resize(ncry);
  fRopTpc.resize(ncry);
  fRopPlane.resize(ncry);

  for (Index icry = 0; icry < ncry; ++icry) {
    Index const napa = ntpc / 2;
    fNApa[icry] = napa;
    fWiresPerPlane[icry].resize(ntpc);
    fAnchoredWires[icry].resize(ntpc);
    fRopsPerApa[icry].resize(napa, 4);
    fPlanesPerRop[icry].resize(napa);
    fPlaneApa[icry].resize(ntpc);
    fPlaneRop[icry].resize(ntpc);
    fPlaneRopIndex[icry].resize(ntpc);
    fFirstChannelInThisRop[icry].resize(napa);
    fFirstChannelInNextRop[icry].resize(napa);
    fRopTpc[icry].resize(napa);
    fRopPlane[icry].resize(napa);

    for (Index itpc = 0; itpc < ntpc; ++itpc) {
      fPlaneApa[icry][itpc].resize(npla, kBadIndex);
      fPlaneRop[icry][itpc].resize(npla, kBadIndex);
      fPlaneRopIndex[icry][itpc].resize(npla, kBadIndex);
      fAnchoredWires[icry][itpc].resize(npla, 0);
      fWiresPerPlane[icry][itpc].resize(npla, 0);

      for (Index ipla = 0; ipla < npla; ++ipla) {
        fWiresPerPlane[icry][itpc][ipla] = kWiresPerPlane[ipla];
      }
    }

    Index itpc = 0;
    for (Index iapa = 0; iapa < napa; ++iapa) {
      Index const nrop = fRopsPerApa[icry][iapa];
      fFirstChannelInThisRop[icry][iapa].resize(nrop, 0);
      fFirstChannelInNextRop[icry][iapa].resize(nrop, 0);
      fPlanesPerRop[icry][iapa].resize(nrop, 0);
      fRopTpc[icry][iapa].resize(nrop);
      fRopPlane[icry][iapa].resize(nrop);

      Index ipla = 0;
      for (Index irop = 0; irop < 2; ++irop) {
        fPlanesPerRop[icry][iapa][irop] = 2;
        fRopTpc[icry][iapa][irop].push_back(itpc);
        fRopTpc[icry][iapa][irop].push_back(itpc + 1);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        ++ipla;
      }

      for (Index irop = 2; irop < 4; ++irop) {
        fPlanesPerRop[icry][iapa][irop] = 1;
        fRopTpc[icry][iapa][irop].push_back(itpc);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        ++itpc;
      }
    }
  }

  raw::ChannelID_t icha = 0;
  for (Index icry = 0; icry < ncry; ++icry) {
    Index const napa = fNApa[icry];
    for (Index iapa = 0; iapa < napa; ++iapa) {
      Index const nrop = fRopsPerApa[icry][iapa];
      for (Index irop = 0; irop < nrop; ++irop) {
        Index const nrpl = fPlanesPerRop[icry][iapa][irop];
        fFirstChannelInThisRop[icry][iapa][irop] = icha;

        for (Index irpl = 0; irpl < nrpl; ++irpl) {
          Index const itpc = fRopTpc[icry][iapa][irop][irpl];
          Index const ipla = fRopPlane[icry][iapa][irop][irpl];

          fPlaneApa[icry][itpc][ipla] = iapa;
          fPlaneRop[icry][itpc][ipla] = irop;
          fPlaneRopIndex[icry][itpc][ipla] = irpl;

          Index nAnchoredWires = 0;
          Index const nwir = fWiresPerPlane[icry][itpc][ipla];
          View_t const view = kPlaneViews[ipla];
          if (view == geo::kZ) {
            nAnchoredWires = nwir;
          } else if (nwir > 310 && nwir < 320) {
            nAnchoredWires = 200;
          }

          if (nAnchoredWires == 0) {
            throw make_runtime_error("Unable to determine anchored wire count for ICEBERG plane.");
          }

          fAnchoredWires[icry][itpc][ipla] = nAnchoredWires;
          icha += nAnchoredWires;
        }

        fFirstChannelInNextRop[icry][iapa][irop] = icha;
      }
    }
  }

  if (icha != fNchannels) {
    std::ostringstream ss;
    ss << "ICEBERG channel map built " << icha << " channels but expected " << fNchannels;
    throw make_runtime_error(ss.str());
  }

  TLOG_DEBUG("IcebergWireChannelMap") << "Initialized ICEBERG wire/channel map with " << fNchannels
                                      << " channels.";
}

void IcebergWireChannelMap::validateWireID(WireID const& wire_id) const
{
  Index const icry = wire_id.Cryostat;
  Index const itpc = wire_id.TPC;
  Index const ipla = wire_id.Plane;

  if (icry >= fNcryostat) {
    throw make_out_of_range("Invalid cryostat in WireID: " + format_wire_id(wire_id));
  }
  if (itpc >= fWiresPerPlane[icry].size()) {
    throw make_out_of_range("Invalid TPC in WireID: " + format_wire_id(wire_id));
  }
  if (ipla >= fWiresPerPlane[icry][itpc].size()) {
    throw make_out_of_range("Invalid plane in WireID: " + format_wire_id(wire_id));
  }
  if (wire_id.Wire >= fWiresPerPlane[icry][itpc][ipla]) {
    throw make_out_of_range("Invalid wire in WireID: " + format_wire_id(wire_id));
  }
}

std::vector<WireID> IcebergWireChannelMap::ChannelToWire(raw::ChannelID_t channel) const
{
  std::vector<WireID> wire_ids;
  if (channel >= fNchannels) {
    return wire_ids;
  }

  Index const ncry = fNcryostat;
  Index icry = kBadIndex;
  Index iapa = kBadIndex;
  Index irop = kBadIndex;
  Index ichaRop = kBadIndex;
  bool found = false;

  for (icry = 0; icry < ncry; ++icry) {
    Index const napa = fNApa[icry];
    for (iapa = 0; iapa < napa; ++iapa) {
      Index const nrop = fRopsPerApa[icry][iapa];
      for (irop = 0; irop < nrop; ++irop) {
        Index const icha1 = fFirstChannelInThisRop[icry][iapa][irop];
        Index const icha2 = fFirstChannelInNextRop[icry][iapa][irop];
        found = channel >= icha1 && channel < icha2;
        if (found) {
          ichaRop = channel - icha1;
          break;
        }
      }
      if (found) {
        break;
      }
    }
    if (found) {
      break;
    }
  }

  if (!found) {
    std::ostringstream ss;
    ss << "Unable to find APA plane for channel " << channel;
    throw make_out_of_range(ss.str());
  }

  Index const nrpl = fPlanesPerRop[icry][iapa][irop];
  if (nrpl == 0 || nrpl > 2) {
    throw make_runtime_error("ICEBERG channel map has invalid ROP plane count.");
  }

  Index const itpc1 = fRopTpc[icry][iapa][irop][0];
  Index const ipla = fRopPlane[icry][iapa][irop][0];
  Index const itpc2 = (nrpl > 1) ? fRopTpc[icry][iapa][irop][1] : itpc1;
  Index const nAnchored = fAnchoredWires[icry][itpc1][ipla];
  bool const wrapped = ipla < 2;

  if (wrapped && (itpc2 == itpc1 || ipla != fRopPlane[icry][iapa][irop][1])) {
    throw make_runtime_error("ICEBERG wrapped plane metadata is inconsistent.");
  }
  if (wrapped && fAnchoredWires[icry][itpc2][ipla] != nAnchored) {
    throw make_runtime_error("ICEBERG wrapped planes have inconsistent anchor counts.");
  }

  Index itpc = itpc1;
  Index iwir = ichaRop;
  if (wrapped && iwir >= nAnchored) {
    itpc = itpc2;
    iwir -= nAnchored;
  }
  if (iwir >= nAnchored) {
    std::ostringstream ss;
    ss << "Invalid channel-to-wire state for channel " << channel;
    throw make_runtime_error(ss.str());
  }

  while (iwir < fWiresPerPlane[icry][itpc][ipla]) {
    wire_ids.emplace_back(icry, itpc, ipla, iwir);
    iwir += fAnchoredWires[icry][itpc][ipla];
    itpc = (itpc == itpc1) ? itpc2 : itpc1;
  }

  return wire_ids;
}

raw::ChannelID_t IcebergWireChannelMap::PlaneWireToChannel(WireID const& wire_id) const
{
  validateWireID(wire_id);

  Index const icry = wire_id.Cryostat;
  Index const itpc = wire_id.TPC;
  Index const ipla = wire_id.Plane;
  Index ichaRop = wire_id.Wire;
  Index const iapa = fPlaneApa[icry][itpc][ipla];
  Index const irop = fPlaneRop[icry][itpc][ipla];
  Index const irpl = fPlaneRopIndex[icry][itpc][ipla];

  if (iapa == kBadIndex || irop == kBadIndex || irpl == kBadIndex) {
    throw make_runtime_error("Wire plane is not assigned to a valid ROP: " + format_wire_id(wire_id));
  }

  Index ncha = fAnchoredWires[icry][itpc][ipla];
  Index const nrpl = fPlanesPerRop[icry][iapa][irop];
  if (nrpl > 1) {
    Index const ipla1 = fRopPlane[icry][iapa][irop][0];
    Index const ipla2 = fRopPlane[icry][iapa][irop][1];
    if (irpl == 1) {
      ichaRop += fAnchoredWires[icry][itpc][ipla1];
      ncha += fAnchoredWires[icry][itpc][ipla1];
    } else {
      ncha += fAnchoredWires[icry][itpc][ipla2];
    }
  }

  Index const icha1 = fFirstChannelInThisRop[icry][iapa][irop];
  return icha1 + ichaRop % ncha;
}

} // namespace geo