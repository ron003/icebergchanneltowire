#ifndef ICEBERGCHANNELOWIRE_APPS_ICEBERGWIRECHANNELMAP_HPP
#define ICEBERGCHANNELOWIRE_APPS_ICEBERGWIRECHANNELMAP_HPP

#include <limits>
#include <vector>

#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

namespace geo {

class IcebergWireChannelMap {
public:
  using Index = unsigned int;

  IcebergWireChannelMap();

  std::vector<WireID> ChannelToWire(raw::ChannelID_t channel) const;
  raw::ChannelID_t PlaneWireToChannel(WireID const& wire_id) const;
  unsigned int Nchannels() const noexcept { return fNchannels; }

private:
  template <typename T>
  using TPCInfoMap_t = std::vector<std::vector<T>>;

  template <typename T>
  using PlaneInfoMap_t = TPCInfoMap_t<std::vector<T>>;

  template <typename T>
  using Vector = std::vector<T>;

  template <typename T>
  using TwoVector = Vector<Vector<T>>;

  template <typename T>
  using ThreeVector = Vector<TwoVector<T>>;

  template <typename T>
  using FourVector = Vector<ThreeVector<T>>;

  static constexpr Index kBadIndex = std::numeric_limits<Index>::max();

  void validateWireID(WireID const& wire_id) const;

  unsigned int fNcryostat = 0;
  unsigned int fNchannels = 0;

  std::vector<unsigned int> fNApa;

  PlaneInfoMap_t<raw::ChannelID_t> fFirstChannelInThisRop;
  PlaneInfoMap_t<raw::ChannelID_t> fFirstChannelInNextRop;

  TwoVector<unsigned int> fRopsPerApa;
  ThreeVector<unsigned int> fPlanesPerRop;
  ThreeVector<unsigned int> fWiresPerPlane;
  ThreeVector<unsigned int> fAnchoredWires;
  ThreeVector<unsigned int> fPlaneApa;
  ThreeVector<unsigned int> fPlaneRop;
  ThreeVector<unsigned int> fPlaneRopIndex;
  FourVector<unsigned int> fRopTpc;
  FourVector<unsigned int> fRopPlane;
};

} // namespace geo

#endif