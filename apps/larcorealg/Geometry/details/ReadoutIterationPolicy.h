#ifndef LARCOREALG_GEOMETRY_DETAILS_READOUTITERATIONPOLICY_H
#define LARCOREALG_GEOMETRY_DETAILS_READOUTITERATIONPOLICY_H

// LArSoft libraries
#include "larcorealg/Geometry/details/GeometryIterationPolicy.h"
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/readout_types.h"

namespace geo::details {
  class ReadoutIterationPolicy : public GeometryIterationPolicy {
  public:
    ReadoutIterationPolicy() = default; // Required for subranges
    ReadoutIterationPolicy(GeometryCore const* geom, WireReadoutGeom const* wireReadoutGeom);

    using GeometryIterationPolicy::NSiblings;
    unsigned int NSiblings(PlaneID const& id) const;
    unsigned int NSiblings(WireID const& id) const;
    unsigned int NSiblings(readout::TPCsetID const& id) const;
    unsigned int NSiblings(readout::ROPID const& id) const;

    template <typename GeoID>
    GeoID GetEndID() const;

    template <typename GeoID, typename ContextID>
    GeoID GetEndID(ContextID const& id) const;

  private:
    PlaneID EndPlaneID() const;
    PlaneID EndPlaneID(CryostatID const& id) const;
    PlaneID EndPlaneID(TPCID const& id) const;

    WireID EndWireID() const;
    WireID EndWireID(CryostatID const& id) const;
    WireID EndWireID(TPCID const& id) const;
    WireID EndWireID(PlaneID const& id) const;

    readout::TPCsetID EndTPCsetID() const;
    readout::TPCsetID EndTPCsetID(CryostatID const& id) const;

    readout::ROPID EndROPID() const;
    readout::ROPID EndROPID(CryostatID const& id) const;
    readout::ROPID EndROPID(readout::TPCsetID const& id) const;

    GeometryCore const* fGeom{nullptr};
    WireReadoutGeom const* fWireReadoutGeom{nullptr};
  };

  // PlaneID
  template <>
  inline PlaneID ReadoutIterationPolicy::GetEndID<PlaneID>() const
  {
    return EndPlaneID();
  }

  template <>
  inline PlaneID ReadoutIterationPolicy::GetEndID<PlaneID, CryostatID>(CryostatID const& id) const
  {
    return EndPlaneID(id);
  }

  template <>
  inline PlaneID ReadoutIterationPolicy::GetEndID<PlaneID, TPCID>(TPCID const& id) const
  {
    return EndPlaneID(id);
  }

  // WireID
  template <>
  inline WireID ReadoutIterationPolicy::GetEndID<WireID>() const
  {
    return EndWireID();
  }

  template <>
  inline WireID ReadoutIterationPolicy::GetEndID<WireID, CryostatID>(CryostatID const& id) const
  {
    return EndWireID(id);
  }

  template <>
  inline WireID ReadoutIterationPolicy::GetEndID<WireID, TPCID>(TPCID const& id) const
  {
    return EndWireID(id);
  }

  template <>
  inline WireID ReadoutIterationPolicy::GetEndID<WireID, PlaneID>(PlaneID const& id) const
  {
    return EndWireID(id);
  }

  // TPCsetID
  template <>
  inline readout::TPCsetID ReadoutIterationPolicy::GetEndID<readout::TPCsetID>() const
  {
    return EndTPCsetID();
  }

  template <>
  inline readout::TPCsetID ReadoutIterationPolicy::GetEndID<readout::TPCsetID, CryostatID>(
    CryostatID const& id) const
  {
    return EndTPCsetID(id);
  }

  // ROPID
  template <>
  inline readout::ROPID ReadoutIterationPolicy::GetEndID<readout::ROPID>() const
  {
    return EndROPID();
  }

  template <>
  inline readout::ROPID ReadoutIterationPolicy::GetEndID<readout::ROPID, CryostatID>(
    CryostatID const& id) const
  {
    return EndROPID(id);
  }

  template <>
  inline readout::ROPID ReadoutIterationPolicy::GetEndID<readout::ROPID, readout::TPCsetID>(
    readout::TPCsetID const& id) const
  {
    return EndROPID(id);
  }

  PlaneGeo const* getElementPtr(WireReadoutGeom const* wireReadoutGeom, PlaneID const& id);
  WireGeo const* getElementPtr(WireReadoutGeom const* wireReadoutGeom, WireID const& id);

  bool validElement(WireReadoutGeom const* geom, PlaneID const& id);
  bool validElement(WireReadoutGeom const* wireReadoutGeom, WireID const& id);
}

#endif // LARCOREALG_GEOMETRY_DETAILS_READOUTITERATIONPOLICY_H
