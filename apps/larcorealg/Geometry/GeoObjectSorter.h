////////////////////////////////////////////////////////////////////////
/// @file  GeoObjectSorter.h
/// @brief Interface to algorithm class for sorting geo::XXXGeo objects
/// @ingroup Geometry
////////////////////////////////////////////////////////////////////////

#ifndef LARCOREALG_GEOMETRY_GEOOBJECTSORTER_H
#define LARCOREALG_GEOMETRY_GEOOBJECTSORTER_H

#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

#include <vector>

namespace geo {

  class GeoObjectSorter {
  public:
    virtual ~GeoObjectSorter() = default;
    void sort(std::vector<CryostatGeo>& cryostats) const;
    void sort(std::vector<TPCGeo>& tpcs) const;
    void sort(std::vector<OpDetGeo>& ods) const;

  private:
    virtual bool compareCryostats(CryostatGeo const& c1, CryostatGeo const& c2) const = 0;
    virtual bool compareTPCs(TPCGeo const& t1, TPCGeo const& t2) const = 0;

    virtual bool compareOpDets(OpDetGeo const& od1, OpDetGeo const& od2) const;
  };

}

#endif // LARCOREALG_GEOMETRY_GEOOBJECTSORTER_H
