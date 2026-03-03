/**
 * @file   larcorealg/Geometry/LocalTransformation.tcc
 * @brief  Class containing local-to-world transformations
 *         (template implementation)
 * @see    LocalTransformation.h
 *
 * This file is expected to be included directly in the header.
 */

#ifndef LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_TCC
#define LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_TCC

// framework libraries
//#include "cetlib_except/exception.h"

// ROOT
#if 0
#include "TGeoMatrix.h"
#include "TGeoNode.h"

// CLHEP
#include "CLHEP/Geometry/Transform3D.h"
#include "CLHEP/Vector/Rotation.h"           // CLHEP::HepRotation
#include "CLHEP/Vector/RotationInterfaces.h" // CLHEP::HepRep3x3
#include "CLHEP/Vector/ThreeVector.h"        // CLHEP::Hep3Vector
#endif

// C standard library
#include <cassert>

//------------------------------------------------------------------------------
namespace geo {
  namespace details {

    //--------------------------------------------------------------------------
    template <typename Dest, typename Src>
    struct TransformationMatrixConverter {
      static decltype(auto) convert(Src const& trans);
      static decltype(auto) convert(Src&& trans);
    };

    //--------------------------------------------------------------------------
    template <typename T, std::size_t SrcN = 3, std::size_t DestN = SrcN>
    bool doBuffersOverlap(T const* src, T const* dest)
    {
      return (dest < (src + SrcN)) && (src < (dest + DestN));
    }

    template <typename T, std::size_t SrcN = 3, std::size_t DestN = SrcN>
    void checkVectorBufferOverlap(T const* src, T const* dest)
    {
      if (doBuffersOverlap<T, SrcN, DestN>(src, dest)) {
        throw cet::exception("LocalTransformation")
          << "source " << SrcN << "@[" << ((void*)src) << "]"
          << " and destination " << DestN << "@[" << ((void*)dest) << "]"
          << " buffers overlap!\n";
      }
      assert(!doBuffersOverlap(src, dest));
    } // checkVectorBufferOverlap()

    //--------------------------------------------------------------------------

  } // namespace details
} // namespace geo

//------------------------------------------------------------------------------
template <typename Matrix>
void geo::LocalTransformation<Matrix>::LocalToWorld(double const* local, double* world) const
{
  details::checkVectorBufferOverlap(local, world);
  fGeoMatrix.LocalToMaster(local, world);
}

//------------------------------------------------------------------------------
template <typename Matrix>
void geo::LocalTransformation<Matrix>::LocalToWorldVect(double const* local, double* world) const
{
  details::checkVectorBufferOverlap(local, world);
  fGeoMatrix.LocalToMasterVect(local, world);
}

//------------------------------------------------------------------------------
template <typename Matrix>
void geo::LocalTransformation<Matrix>::WorldToLocal(double const* world, double* local) const
{
  details::checkVectorBufferOverlap(local, world);
  fGeoMatrix.MasterToLocal(world, local);
}

//------------------------------------------------------------------------------
template <typename Matrix>
void geo::LocalTransformation<Matrix>::WorldToLocalVect(const double* world, double* local) const
{
  details::checkVectorBufferOverlap(local, world);
  fGeoMatrix.MasterToLocalVect(world, local);
}

//------------------------------------------------------------------------------
template <typename Matrix>
template <typename DestPoint, typename SrcPoint>
DestPoint geo::LocalTransformation<Matrix>::WorldToLocalImpl(SrcPoint const& world) const
{
  double const worldArray[3] = {world.X(), world.Y(), world.Z()};
  double localArray[3];
  WorldToLocal(worldArray, localArray);
  return {localArray[0], localArray[1], localArray[2]};
}

//......................................................................
template <typename Matrix>
template <typename DestVector, typename SrcVector>
DestVector geo::LocalTransformation<Matrix>::WorldToLocalVectImpl(SrcVector const& world) const
{
  double const worldArray[3] = {world.X(), world.Y(), world.Z()};
  double localArray[3];
  WorldToLocalVect(worldArray, localArray);
  return {localArray[0], localArray[1], localArray[2]};
}

//......................................................................
template <typename Matrix>
template <typename DestPoint, typename SrcPoint>
DestPoint geo::LocalTransformation<Matrix>::LocalToWorldImpl(SrcPoint const& local) const
{
  double const localArray[3] = {local.X(), local.Y(), local.Z()};
  double worldArray[3];
  LocalToWorld(localArray, worldArray);
  return {worldArray[0], worldArray[1], worldArray[2]};
}

//......................................................................
template <typename Matrix>
template <typename DestVector, typename SrcVector>
DestVector geo::LocalTransformation<Matrix>::LocalToWorldVectImpl(SrcVector const& local) const
{
  double const localArray[3] = {local.X(), local.Y(), local.Z()};
  double worldArray[3];
  LocalToWorldVect(localArray, worldArray);
  return {worldArray[0], worldArray[1], worldArray[2]};
}

//------------------------------------------------------------------------------
// specialisations (template implementations)
//
namespace geo {

  //----------------------------------------------------------------------------
  template <>
  inline TGeoHMatrix transformationFromPath<TGeoHMatrix>(std::vector<GeoNodePathEntry> const& path,
                                                         size_t depth)
  {
    TGeoHMatrix matrix = *(path[0].node->GetMatrix());
    for (size_t i = 1; i <= depth; ++i)
      matrix.Multiply(path[i].node->GetMatrix());
    return matrix;
  }

  template <>
  inline TGeoHMatrix transformationFromPath<TGeoHMatrix, GeoNodeIterator_t>(GeoNodeIterator_t begin,
                                                                            GeoNodeIterator_t end)
  {
    if (begin == end) return {TGeoIdentity()};
    auto iNode = begin;
    TGeoHMatrix matrix = *(iNode->node->GetMatrix());
    while (++iNode != end)
      matrix.Multiply(iNode->node->GetMatrix());
    return matrix;
  }

  //----------------------------------------------------------------------------
  template <>
  inline HepGeom::Transform3D transformationFromPath<HepGeom::Transform3D>(
    std::vector<GeoNodePathEntry> const& path,
    size_t depth)
  {
    auto const mat = transformationFromPath<TGeoHMatrix>(path, depth);
    const Double_t* translation = mat.GetTranslation();
    return HepGeom::Transform3D(CLHEP::HepRotation(CLHEP::HepRep3x3(mat.GetRotationMatrix())),
                                CLHEP::Hep3Vector(translation[0], translation[1], translation[2]));
  }

  template <>
  HepGeom::Transform3D inline transformationFromPath<HepGeom::Transform3D>(GeoNodeIterator_t begin,
                                                                           GeoNodeIterator_t end)
  {
    auto const mat = transformationFromPath<TGeoHMatrix>(begin, end);
    const Double_t* translation = mat.GetTranslation();
    return HepGeom::Transform3D(CLHEP::HepRotation(CLHEP::HepRep3x3(mat.GetRotationMatrix())),
                                CLHEP::Hep3Vector(translation[0], translation[1], translation[2]));
  }

  //----------------------------------------------------------------------------
  namespace details {

    //--------------------------------------------------------------------------
    template <typename Trans>
    struct TransformationMatrixConverter<Trans, Trans> {
      static Trans const& convert(Trans const& trans) { return trans; }
      static Trans convert(Trans&& trans) { return trans; }
    };

    //--------------------------------------------------------------------------

  } // namespace details

  //----------------------------------------------------------------------------

} // namespace geo

//------------------------------------------------------------------------------

#endif // LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_TCC

// Local variables:
// mode: c++
// End:
