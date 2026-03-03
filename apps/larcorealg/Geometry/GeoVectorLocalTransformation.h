/**
 * @file   larcorealg/Geometry/GeoVectorLocalTransformation.h
 * @brief  Specialization of local-to-world transformations for ROOT GenVector.
 * @ingroup Geometry
 *
 * This is a header-only library.
 */

#ifndef LARCOREALG_GEOMETRY_GEOVECTORLOCALTRANSFORMATION_H
#define LARCOREALG_GEOMETRY_GEOVECTORLOCALTRANSFORMATION_H

// LArSoft libraries
#include "larcorealg/Geometry/LocalTransformation.h"

// ROOT libraries
#include "Math/GenVector/Transform3D.h"

// C/C++ standard libraries
#include <stddef.h>
#include <vector>

// ROOT forward declaration (should be redundant)
class TGeoNode;

namespace geo {

  //----------------------------------------------------------------------------
  //---  template specialization forward declarations
  //----------------------------------------------------------------------------
  //---  LocalTransformation<ROOT::Math::Transform3D>
  //----------------------------------------------------------------------------
  template <>
  void LocalTransformation<ROOT::Math::Transform3D>::LocalToWorld(double const* local,
                                                                  double* world) const;

  //------------------------------------------------------------------------------
  template <>
  void LocalTransformation<ROOT::Math::Transform3D>::LocalToWorldVect(double const* local,
                                                                      double* world) const;

  //------------------------------------------------------------------------------
  template <>
  void LocalTransformation<ROOT::Math::Transform3D>::WorldToLocal(double const* world,
                                                                  double* local) const;

  //------------------------------------------------------------------------------
  template <>
  void LocalTransformation<ROOT::Math::Transform3D>::WorldToLocalVect(const double* world,
                                                                      double* local) const;

  //------------------------------------------------------------------------------
  template <>
  template <typename DestPoint, typename SrcPoint>
  DestPoint LocalTransformation<ROOT::Math::Transform3D>::WorldToLocalImpl(
    SrcPoint const& world) const;

  //......................................................................
  template <>
  template <typename DestVector, typename SrcVector>
  DestVector LocalTransformation<ROOT::Math::Transform3D>::WorldToLocalVectImpl(
    SrcVector const& world) const;

  //......................................................................
  template <>
  template <typename DestPoint, typename SrcPoint>
  DestPoint LocalTransformation<ROOT::Math::Transform3D>::LocalToWorldImpl(
    SrcPoint const& local) const;

  //......................................................................
  template <>
  template <typename DestVector, typename SrcVector>
  DestVector LocalTransformation<ROOT::Math::Transform3D>::LocalToWorldVectImpl(
    SrcVector const& local) const;

  //------------------------------------------------------------------------------

} // namespace geo

//------------------------------------------------------------------------------
// template implementation

#include "GeoVectorLocalTransformation.tcc"

//------------------------------------------------------------------------------

#endif // LARCOREALG_GEOMETRY_GEOVECTORLOCALTRANSFORMATION_H
