#ifndef LARCOREALG_GEOMETRY_DETAILS_GEOMETRYITERATIONPOLICY_H
#define LARCOREALG_GEOMETRY_DETAILS_GEOMETRYITERATIONPOLICY_H

// LArSoft libraries
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

namespace geo::details {
  class GeometryIterationPolicy {
  public:
    GeometryIterationPolicy() = default; // Required for subranges
    explicit GeometryIterationPolicy(GeometryCore const* geom);
    unsigned int NSiblings(CryostatID const& id) const;
    unsigned int NSiblings(TPCID const& id) const;

    template <typename GeoID>
    GeoID GetEndID() const;

    template <typename GeoID, typename ContextID>
    GeoID GetEndID(ContextID const& id) const;

  protected:
    CryostatID EndCryostatID() const;

    TPCID EndTPCID() const;
    TPCID EndTPCID(CryostatID const& id) const;

    GeometryCore const* fGeom{nullptr};
  };

  /// Initializes the specified ID with the invalid ID after the last cryostat
  template <>
  inline CryostatID GeometryIterationPolicy::GetEndID<CryostatID>() const
  {
    return EndCryostatID();
  }

  // TPCID
  template <>
  inline TPCID GeometryIterationPolicy::GetEndID<TPCID>() const
  {
    return EndTPCID();
  }

  template <>
  inline TPCID GeometryIterationPolicy::GetEndID<TPCID, CryostatID>(CryostatID const& id) const
  {
    return EndTPCID(id);
  }

  CryostatGeo const* getElementPtr(GeometryCore const* geom, CryostatID const& id);
  TPCGeo const* getElementPtr(GeometryCore const* geom, TPCID const& id);

  bool validElement(GeometryCore const* geom, CryostatID const& id);
  bool validElement(GeometryCore const* geom, TPCID const& id);
}

#endif // LARCOREALG_GEOMETRY_DETAILS_GEOMETRYITERATIONPOLICY_H
