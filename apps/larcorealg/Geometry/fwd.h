#ifndef LARCOREALG_GEOMETRY_FWD_H
#define LARCOREALG_GEOMETRY_FWD_H

#include <functional>

namespace geo {
  class AuxDetGeo;
  class AuxDetGeoObjectSorter;
  class AuxDetGeometryCore;
  class AuxDetSensitiveGeo;
  class CryostatGeo;
  class GeoObjectSorter;
  class GeometryBuilder;
  class GeometryCore;
  class OpDetGeo;
  class PlaneGeo;
  class TPCGeo;
  class WireGeo;
  class WireReadoutGeom;

  template <typename T>
  using Compare = std::function<bool(T const&, T const&)>;
}

#endif // LARCOREALG_GEOMETRY_FWD_H
