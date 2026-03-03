/**
 * @file   larcorealg/Geometry/LocalTransformation.h
 * @brief  Class containing local-to-world transformations
 * @ingroup Geometry
 *
 */

#ifndef LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_H
#define LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_H

// ROOT libraries
class TGeoNode;

// C/C++ standard libraries
#include <cstdlib>     // std::size_t
#include <type_traits> // std::enable_if_t<>, std::decay_t<>
#include <utility>     // std::move(), std::forward()
#include <vector>

// forward declarations
class TGeoNode;

namespace geo {

  namespace details {
    template <typename Dest, typename Src>
    struct TransformationMatrixConverter;
  } // namespace details

  struct GeoNodePathEntry {
    TGeoNode const* node;
    std::size_t hash_value;
  };

  using GeoNodeIterator_t = std::vector<GeoNodePathEntry>::const_iterator;

  /// Builds a matrix to go from local to world coordinates in one step
  template <typename StoredMatrix, typename ITER>
  static StoredMatrix transformationFromPath(ITER begin, ITER end);

  /// Builds a matrix to go from local to world coordinates in one step
  template <typename StoredMatrix>
  static StoredMatrix transformationFromPath(std::vector<GeoNodePathEntry> const& path,
                                             size_t depth);

  /**
   * @brief Class to transform between world and local coordinates
   * @tparam StoredMatrix type of transformation matrix internally stored
   * @ingroup Geometry
   *
   * This class provides two directions of transformations (world to local and
   * the other way around), for points and for vectors.
   * The vector version of the transformation does not apply translation.
   *
   * @note In the class method examples, the following definition is assumed:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * using LocalTransformation_t = geo::LocalTransformation<TGeoHMatrix>;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   */
  template <typename StoredMatrix>
  class LocalTransformation {
  public:
    /// Type of transformation matrix
    using TransformationMatrix_t = StoredMatrix;

    //@{
    /**
     * @brief Constructor: uses the specified local-to-world transformation.
     * @param matrix the transformation matrix to be used
     *
     * The transformation `matrix` is used to transform vectors from the "local"
     * to the "world" frame, while its inverse is used for transformations from
     * the "world" to the "local" frame.
     *
     * The specified matrix is copied into a local copy unless a R-value
     * reference argument is specified (e.g. with `std::move()`).
     */
    LocalTransformation(TransformationMatrix_t const& matrix) : fGeoMatrix(matrix) {}
    LocalTransformation(TransformationMatrix_t&& matrix) : fGeoMatrix(std::move(matrix)) {}
    //@}

    /**
     * @brief Constructor: chains the transformations from the specified nodes.
     * @param path the path of ROOT geometry nodes
     * @param depth the index in the path of the last node to be considered
     *
     * The resulting transformation is the sequence of transformations from
     * `depth` nodes from the first on.
     */
    LocalTransformation(std::vector<GeoNodePathEntry> const& path, size_t depth)
      : fGeoMatrix(transformationFromPath<StoredMatrix>(path.begin(), path.begin() + depth + 1))
    {}

    /**
     * @brief Constructor: chains the transformations from all specified nodes.
     * @param path the path of ROOT geometry nodes
     *
     * The resulting transformation is the sequence of transformations from
     * the first to the last node of the path.
     */
    LocalTransformation(std::vector<GeoNodePathEntry> const& path)
      : LocalTransformation(path, path.size())
    {}

    /**
     * @brief Constructor: sequence of transformations from a node path.
     * @tparam ITER type of iterator to node pointers
     * @param begin the begin iterator of the path of ROOT geometry nodes
     * @param end the end iterator of the path of ROOT geometry nodes
     *
     * The resulting transformation is the sequence of transformations from
     * the one pointed by `begin` to the one before `end`.
     */
    template <typename ITER>
    LocalTransformation(ITER begin, ITER end)
      : fGeoMatrix(transformationFromPath<StoredMatrix>(begin, end))
    {}

    /**
     * @brief Transforms a point from local frame to world frame
     * @param local local coordinates: [0] x, [1] y, [2] z [cm]
     * @param world (output) corresponding world coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * LocalTransformation_t trans( ... ); // with proper initialisation
     *
     * std::array<double, 3U> origin, center;
     * origin.fill(0.);
     * trans.LocalToWorld(origin.data(), center.data());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `center` will contain the world coordinates of the center of the volume,
     * which is usually represented by the origin in the local coordinates.
     *
     * In-place replacement is *not* supported: `world` and `local` buffers are
     * assumed not to, and must not, overlap.
     */
    void LocalToWorld(double const* local, double* world) const;

    // @{
    /**
     * @brief Transforms a point from local frame to world frame
     * @tparam SrcPoint type of the input (local) vector
     * @tparam DestPoint type of the output (world) vector (default: as `Point`)
     * @param local local coordinates [cm]
     * @return corresponding world coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * LocalTransformation_t trans( ... ); // with proper initialisation
     *
     * auto center = trans.LocalToWorld(TVector3());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `center` will be a `TVector3` containing the world coordinates of the
     * center of the volume, which is usually represented by the origin in the
     * local coordinates (a TVector3 is by default constructed to point to the
     * origin).
     */
    template <typename DestPoint,
              typename SrcPoint,
              typename = std::enable_if_t<!std::is_same<SrcPoint, DestPoint>::value>>
    DestPoint LocalToWorld(SrcPoint const& local) const
    {
      return LocalToWorldImpl<DestPoint>(local);
    }
    template <typename Point>
    Point LocalToWorld(Point const& local) const
    {
      return LocalToWorldImpl<Point>(local);
    }
    // @}

    /**
     * @brief Transforms a vector from local frame to world frame
     * @param local local coordinates: [0] x, [1] y, [2] z [cm]
     * @param world (output) corresponding world coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     *
     * In-place replacement is *not* supported: `world` and `local` buffers are
     * assumed not to, and must not, overlap.
     */
    void LocalToWorldVect(double const* local, double* world) const;

    //@{
    /**
     * @brief Transforms a vector from local frame to world frame
     * @tparam SrcVector type of the input (local) vector
     * @tparam DestVector type of output (world) vector (default: as `Vector`)
     * @param local local coordinates [cm]
     * @return corresponding world coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     */
    template <typename DestVector,
              typename SrcVector,
              typename = std::enable_if_t<!std::is_same<SrcVector, DestVector>::value>>
    DestVector LocalToWorldVect(SrcVector const& local) const
    {
      return LocalToWorldVectImpl<DestVector>(local);
    }
    template <typename Vector>
    Vector LocalToWorldVect(Vector const& local) const
    {
      return LocalToWorldVectImpl<Vector>(local);
    }
    //@}

    /**
     * @brief Transforms a point from world frame to local frame
     * @param world world coordinates: [0] x, [1] y, [2] z [cm]
     * @param local (output) corresponding local coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * LocalTransformation_t trans( ... ); // with proper initialisation
     *
     * std::array<double, 3U> world{ 4.0, 5.0, -2.5 }, local;
     * trans.WorldToLocal(world.data(), local.data());
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `local` will contain the local coordinates of the specified point.
     *
     * In-place replacement is *not* supported: `world` and `local` buffers are
     * assumed not to, and must not, overlap.
     */
    void WorldToLocal(double const* world, double* local) const;

    //@{
    /**
     * @brief Transforms a point from world frame to local frame
     * @tparam SrcPoint type of the input (local) vector
     * @tparam DestPoint type of the output (world) vector (default: as `Point`)
     * @param world world coordinates [cm]
     * @return corresponding local coordinates [cm]
     *
     * The full transformation is applied. Fox example:
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * LocalTransformation_t trans( ... ); // with proper initialisation
     *
     * auto local = trans.WorldToLocal(TVector3(4.0, 5.0, -2.5));
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * `local` will be a `TVector3` containing the local coordinates of the
     * specified point.
     */
    template <typename DestPoint,
              typename SrcPoint,
              typename = std::enable_if_t<!std::is_same<SrcPoint, DestPoint>::value>>
    DestPoint WorldToLocal(SrcPoint const& world) const
    {
      return WorldToLocalImpl<DestPoint>(world);
    }
    template <typename Point>
    Point WorldToLocal(Point const& world) const
    {
      return WorldToLocalImpl<Point>(world);
    }
    //@}

    /**
     * @brief Transforms a vector from world frame to local frame
     * @param world world coordinates: [0] x, [1] y, [2] z [cm]
     * @param local (output) corresponding local coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     *
     * In-place replacement is *not* supported: `world` and `local` buffers are
     * assumed not to, and must not, overlap.
     */
    void WorldToLocalVect(const double* world, double* local) const;

    //@{
    /**
     * @brief Transforms a vector from world frame to local frame
     * @tparam SrcVector type of the input (local) vector
     * @tparam DestVector type of output (world) vector (default: as `Vector`)
     * @param world coordinates [cm]
     * @return corresponding world coordinates [cm]
     *
     * The translation is not applied, since the argument is supposed to be a
     * vector, relative difference between two points.
     */
    template <typename DestVector,
              typename SrcVector,
              typename = std::enable_if_t<!std::is_same<SrcVector, DestVector>::value>>
    DestVector WorldToLocalVect(SrcVector const& world) const
    {
      return WorldToLocalVectImpl<DestVector>(world);
    }
    template <typename Vector>
    Vector WorldToLocalVect(Vector const& world) const
    {
      return WorldToLocalVectImpl<Vector>(world);
    }
    //@}

    /// Direct access to the transformation matrix
    TransformationMatrix_t const& Matrix() const { return fGeoMatrix; }

  protected:
    TransformationMatrix_t fGeoMatrix; ///< local to world transform

    template <typename DestPoint, typename SrcPoint>
    DestPoint LocalToWorldImpl(SrcPoint const& local) const;

    template <typename DestVector, typename SrcVector>
    DestVector LocalToWorldVectImpl(SrcVector const& local) const;

    template <typename DestPoint, typename SrcPoint>
    DestPoint WorldToLocalImpl(SrcPoint const& world) const;

    template <typename DestVector, typename SrcVector>
    DestVector WorldToLocalVectImpl(SrcVector const& world) const;

  }; // class LocalTransformation<>

  /// Converts a transformation matrix into `Dest` format.
  template <typename Dest, typename Src>
  decltype(auto) convertTransformationMatrix(Src&& trans)
  {
    return details::TransformationMatrixConverter<std::decay_t<Dest>, std::decay_t<Src>>::convert(
      std::forward<Src>(trans));
  }

} // namespace geo

//------------------------------------------------------------------------------
// template implementation

#include "LocalTransformation.tcc"

//------------------------------------------------------------------------------

#endif // LARCOREALG_GEOMETRY_LOCALTRANSFORMATION_H
