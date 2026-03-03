/**
 * @file   larcorealg/Geometry/LocalTransformationGeo.h
 * @brief  Local-to-world transformations with LArSoft geometry vectors.
 *
 */

#ifndef LARCOREALG_GEOMETRY_LOCALTRANSFORMATIONGEO_H
#define LARCOREALG_GEOMETRY_LOCALTRANSFORMATIONGEO_H

// LArSoft libraries
#include "larcorealg/Geometry/GeoVectorLocalTransformation.h"
#include "larcorealg/Geometry/LocalTransformation.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_vectors.h" // geo::Point_t...

// C/C++ standard libraries
#include <type_traits> // std::is_same
#include <utility>     // std::move()

namespace geo {

  /**
   * @brief Class to transform between world and local coordinates
   * @tparam StoredMatrix type of transformation matrix internally stored
   * @tparam LocalPoint type representing a local point
   * @tparam LocalVector type representing a local displacement vector
   * @see `geo::LocalTransformation`
   *
   * This class provides two directions of transformations (world to local and
   * the other way around), for points and for vectors.
   *
   * Compared to `geo::LocalTransformation`, this class offers a simplified
   * interface for the supported vectors: `toWorldCoords()` and
   * `toLocalCoords()` apply the correct transformation depending on whether
   * the argument is a point or a displacement vector.
   *
   * @note In the class method examples, the following definition is assumed:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * using LocalTransformationGeo_t = geo::LocalTransformationGeo
   *   <TGeoHMatrix, geo::Point_t, geo::Vector_t>;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * This is conceptually wrong since `geo::Point_t` and `geo::Vector_t` are
   * explicitly tagged to be in the global coordinate frame, but mechanically
   * it will work just the same.
   */
  template <typename StoredMatrix, typename LocalPoint = Point_t, typename LocalVector = Vector_t>
  class LocalTransformationGeo : public LocalTransformation<StoredMatrix> {
    using Base_t = LocalTransformation<StoredMatrix>;

  public:
    using GlobalPoint_t = Point_t;     ///< Type for global 3D point.
    using GlobalVector_t = Vector_t;   ///< Type for global 3D displacement.
    using LocalPoint_t = LocalPoint;   ///< Type for local 3D point.
    using LocalVector_t = LocalVector; ///< Type for local 3D displacement.

    using typename Base_t::TransformationMatrix_t;

    //@{
    /**
     * @brief Constructor: uses the specified transformation matrix.
     * @param matrix the transformation matrix to be used
     *
     * The specified matrix is copied into a local copy.
     */
    LocalTransformationGeo(TransformationMatrix_t const& matrix) : Base_t(matrix) {}
    LocalTransformationGeo(TransformationMatrix_t&& matrix) : Base_t(std::move(matrix)) {}
    //@}

    /**
     * @brief Constructor: uses the specified transformation matrix.
     * @param path the path of ROOT geometry nodes
     * @param depth the index in the path of the last node to be considered
     *
     * The specified matrix is copied into a local copy.
     */
    LocalTransformationGeo(std::vector<TGeoNode const*> const& path, size_t depth)
      : Base_t(path, depth)
    {}

    /**
     * @brief Transforms a point from local frame to world frame.
     * @param local local coordinates [cm]
     * @return corresponding world coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * geo::LocalTransformationGeo_t trans( ... ); // with proper initialisation
     *
     * auto center = trans.toWorldCoords(geo::origin());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `center` will be a `geo::Point_t` containing the world coordinates of
     * the center of the volume, which is usually represented by the origin in
     * the local coordinates.
     */
    GlobalPoint_t toWorldCoords(LocalPoint_t const& local) const
    {
      return Base_t::template LocalToWorld<GlobalPoint_t>(local);
    }

    /**
     * @brief Transforms a vector from local frame to world frame.
     * @param local local coordinates [cm]
     * @return corresponding world coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     */
    GlobalVector_t toWorldCoords(LocalVector_t const& local) const
    {
      return Base_t::template LocalToWorldVect<GlobalVector_t>(local);
    }

    /**
     * @brief Transforms a point from world frame to local frame.
     * @param world world coordinates [cm]
     * @return corresponding local coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * geo::LocalTransformation trans( ... ); // with proper initialisation
     * geo::Point_t p = { 4.0, 5.0, -2.5 };
     * auto local = trans.toLocalCoords(p);
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `local` will be a `LocalPoint_t` containing the local coordinates of the
     * specified point.
     */
    LocalPoint_t toLocalCoords(GlobalPoint_t const& world) const
    {
      return Base_t::template WorldToLocal<LocalPoint_t>(world);
    }

    /**
     * @brief Transforms a vector from world frame to local frame.
     * @param world world coordinates: [0] x, [1] y, [2] z [cm]
     * @return a local vector with corresponding local coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     */
    LocalVector_t toLocalCoords(GlobalVector_t const& world) const
    {
      return Base_t::template WorldToLocalVect<LocalVector_t>(world);
    }

    static_assert(!std::is_same<LocalPoint_t, LocalVector_t>(),
                  "Vector and point types must be distinct");

  }; // class LocalTransformationGeo<>

} // namespace geo

#endif // LARCOREALG_GEOMETRY_LOCALTRANSFORMATIONGEO_H
