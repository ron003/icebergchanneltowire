/**
 * @file   Decomposer.h
 * @brief  Classes to project and compose a vector on a plane
 * @ingroup Geometry
 *
 */

#ifndef LARCOREALG_GEOMETRY_DECOMPOSER_H
#define LARCOREALG_GEOMETRY_DECOMPOSER_H

// LArSoft libraries
#include "larcorealg/Geometry/geo_vectors_utils.h" // geo::vect

// C/C++ standard libraries
#include <cmath>       // std::abs()
#include <type_traits> // std::declval()
#include <utility>     // std::move()

namespace geo {

  // --- BEGIN Decomposition objects -------------------------------------------
  /// @name Decomposition objects
  /// @ingroup Geometry
  /// @{

  /** **************************************************************************
   * @brief A base for a plane in space
   *
   * The base contains the two axes, a "main" one (@f$ \hat{u} @f$) and a
   * "secondary" one (@f$ \hat{v} @f$).
   * It also defines a normal (@f$ \hat{n} @f$) to the plane so that the base
   * is positive defined ((@f$ \hat{u} \times \hat{v} \cdot \hat{m} = +1 @f$).
   *
   */
  template <typename Vector>
  class PlaneBase {
  public:
    using Vector_t = Vector; ///< Type for the vector in space

    /// Rounding threshold for vectors
    static constexpr double RoundingTol = 1e-4;

    /// Constructor: assigns the axes
    PlaneBase(Vector_t const& main, Vector_t const& secondary)
      : fMain(PastorizeUnitVector(main))
      , fSecondary(PastorizeUnitVector(secondary))
      , fNormal(ComputeNormal())
    {}

    /// Returns the main axis direction
    Vector_t const& MainDir() const { return fMain; }

    /// Returns the secondary axis direction
    Vector_t const& SecondaryDir() const { return fSecondary; }

    /// Returns the axis normal to the plane
    Vector_t const& NormalDir() const { return fNormal; }

    /// Change the main direction of the projection base
    void SetMainDir(Vector_t const& dir)
    {
      fMain = dir;
      ResetNormal();
    }

    /// Change the secondary direction of the projection base
    void SetSecondaryDir(Vector_t const& dir)
    {
      fSecondary = dir;
      ResetNormal();
    }

    /// Normalizes and rounds a direction vector
    static Vector_t PastorizeUnitVector(Vector_t dir)
    {
      return vect::rounded01(vect::normalize(dir), RoundingTol);
    }

  private:
    Vector_t fMain;      ///< Main axis on the plane
    Vector_t fSecondary; ///< Secondary axis on the plane
    Vector_t fNormal;    ///< Axis normal to the plane

    /// Computes the normal to the plane
    Vector_t ComputeNormal() const { return PastorizeUnitVector(MainDir().Cross(SecondaryDir())); }

    /// Reset normal to the plane
    void ResetNormal() { fNormal = ComputeNormal(); }

  }; // class PlaneBase<>

  /// A base for a plane in space, with a coordinate system
  /// @tparam Vector type to represent 3D vectors
  /// @tparam Point type to represent 3D points (same as Vector by default)
  template <typename Vector, typename Point = Vector>
  class AffinePlaneBase {
    using PlaneBase_t = PlaneBase<Vector>;

  public:
    using Vector_t = typename PlaneBase_t::Vector_t; ///< Vector in space
    using Point_t = Point;                           ///< Point in space

    /// Constructor: assigns the origin of the system and the axes
    AffinePlaneBase(Point_t const& origin, Vector_t const& main, Vector_t const& secondary)
      : fOrigin(origin), fBase(main, secondary)
    {}

    /// Returns the main axis direction
    Vector_t const& MainDir() const { return fBase.MainDir(); }

    /// Returns the secondary axis direction
    Vector_t const& SecondaryDir() const { return fBase.SecondaryDir(); }

    /// Returns the secondary axis direction
    Vector_t const& NormalDir() const { return fBase.NormalDir(); }

    /// Returns the origin of the coordinate system in world coordinates
    Point_t Origin() const { return fOrigin; }

    /// Returns the vector representing the specified point in the affine space
    Vector_t ToVector(Point_t const& point) const { return point - Origin(); }

    /// Change the 3D point of the reference frame origin
    void SetOrigin(Point_t const& point) { fOrigin = point; }

    /// Change the main direction of the projection base
    void SetMainDir(Vector_t const& dir) { fBase.SetMainDir(dir); }

    /// Change the secondary direction of the projection base
    void SetSecondaryDir(Vector_t const& dir) { fBase.SetSecondaryDir(dir); }

  private:
    Point_t fOrigin;   ///< Origin of the coordinate system
    PlaneBase_t fBase; ///< Base

  }; // class AffinePlaneBase<>

  /// Structure hosting projections of a 3D vector (or point ) on the plane
  /// @tparam ProjVector type for 2D vector projection
  template <typename ProjVector>
  struct DecomposedVector {

    using Projection_t = ProjVector; ///< Type for 2D projection

    /// Type for distance from plane
    using Distance_t = decltype(vect::mag2(std::declval<Projection_t>()));

    Projection_t projection; ///< Projection of the vector on the plane
    Distance_t distance;     ///< Distance of the vector from the plane

    DecomposedVector() = default;

    DecomposedVector(Projection_t const& projection, Distance_t distance)
      : projection(projection), distance(distance)
    {}

    DecomposedVector(Distance_t distance, Projection_t const& projection)
      : projection(projection), distance(distance)
    {}

  }; // struct DecomposedVector

  /** **************************************************************************
   * @brief Class with methods for projection of vectors on a plane
   * @tparam Vector type to represent 3D vectors
   * @tparam Point type to represent 3D points
   * @tparam ProjVector type to represent 2D projection on plane
   *
   * These methods deal with projection of points and vectors on a plane.
   *
   * The plane is defined in a 3D space, with two axes, the "main" and the
   * "auxiliary" one, which are orthogonal.
   */
  template <typename Vector, typename Point, typename ProjVector>
  class PlaneDecomposer {

  public:
    using AffinePlaneBase_t = AffinePlaneBase<Vector, Point>;

    /// Type of decomposed vector
    using DecomposedVector_t = DecomposedVector<ProjVector>;

    /// Type for a point
    using Point_t = typename AffinePlaneBase_t::Point_t;

    /// Type for a vector
    using Vector_t = typename AffinePlaneBase_t::Vector_t;

    /// Type representing the projection vector
    using Projection_t = typename DecomposedVector_t::Projection_t;

    /// Type representing the signed distance from the projection plane
    using Distance_t = typename DecomposedVector_t::Distance_t;

    /// Default constructor: projection on (x,y) with origin (0, 0, 0)
    PlaneDecomposer()
      : fPlaneBase({0.0, 0.0, 0.0}, // origin
                   {1.0, 0.0, 0.0}, // x axis
                   {0.0, 1.0, 0.0}  // y axis
        )
    {}

    /// Constructor: specifies a base (an origin and two direction vectors)
    PlaneDecomposer(AffinePlaneBase_t&& base) : fPlaneBase(std::move(base)) {}

    /// Constructor: specifies a base (an origin and two direction vectors)
    PlaneDecomposer(AffinePlaneBase_t const& base) : fPlaneBase(base) {}

    /// @{
    /// @name Setters

    /// Change projection base
    void SetBase(AffinePlaneBase_t&& base) { fPlaneBase = std::move(base); }

    /// Change projection base
    void SetBase(AffinePlaneBase_t const& base) { fPlaneBase = base; }

    /// Change the 3D point of the reference frame origin
    void SetOrigin(Point_t const& point) { fPlaneBase.SetOrigin(point); }

    /// Change the main direction of the projection base
    void SetMainDir(Vector_t const& dir) { fPlaneBase.SetMainDir(dir); }

    /// Change the secondary direction of the projection base
    void SetSecondaryDir(Vector_t const& dir) { fPlaneBase.SetSecondaryDir(dir); }

    /// @}

    /// @{
    /// @name Reference point and base

    /// Returns the reference point for the plane coordinate, as a 3D point
    Point_t ReferencePoint() const { return Base().Origin(); }

    /// Returns the plane main axis direction
    Vector_t const& MainDir() const { return Base().MainDir(); }

    /// Returns the plane secondary axis direction
    Vector_t const& SecondaryDir() const { return Base().SecondaryDir(); }

    /// Returns the complete base representation
    AffinePlaneBase_t const& Base() const { return fPlaneBase; }

    /// @}

    /// @{
    /// @name Projection coordinate access
    ///
    /// These methods act on 2D (vector) projections.
    ///

    /// Returns the main component of a projection vector
    auto MainComponent(Projection_t const& v) const { return v.X(); }

    /// Returns the secondary component of a projection vector
    auto SecondaryComponent(Projection_t const& v) const { return v.Y(); }

    /// @}

    /// @{
    /// @name Projection on plane

    /// Returns the main component of a 3D point
    auto PointMainComponent(Point_t const& point) const
    {
      return VectorMainComponent(Base().ToVector(point));
    }

    /// Returns the secondary component of a 3D point
    auto PointSecondaryComponent(Point_t const& point) const
    {
      return VectorSecondaryComponent(Base().ToVector(point));
    }

    /**
     * @brief Returns the projection of the specified point on the plane
     * @param point the 3D point to be projected, in world coordinates
     * @return a 2D vector representing the projection of point on the plane
     *
     * The returned vector is a 2D vector expressing the projection of the point
     * (from world coordinates) on the plane.
     * The vector is expressed as @f$ ( m, s ) @f$, components following the
     * main and the secondary direction, respectively.
     * The origin point is the one returned by `ReferencePoint()`.
     */
    Projection_t PointProjection(Point_t const& point) const
    {
      return VectorProjection(Base().ToVector(point));
    }

    /// Returns the main component of a projection vector
    auto VectorMainComponent(Vector_t const& v) const { return vect::dot(v, MainDir()); }

    /// Returns the secondary component of a projection vector
    auto VectorSecondaryComponent(Vector_t const& v) const { return vect::dot(v, SecondaryDir()); }

    /**
     * @brief Returns the projection of the specified vector on the plane
     * @param v the 3D vector to be projected, in world units
     * @return a 2D vector representing the projection of v on the plane
     *
     * The returned vector is a 2D vector expressing the projection of the
     * vector (from world units) on the plane.
     * The vector is expressed as @f$ ( m, s ) @f$, components following the
     * main and the secondary direction, respectively.
     */
    Projection_t VectorProjection(Vector_t const& v) const
    {
      return {VectorMainComponent(v), VectorSecondaryComponent(v)};
    }

    /**
     * @brief Returns the angle of the projection from main direction.
     * @param v vector to get the angle of
     * @return the angle of the projection from main direction, in radians
     *
     * The projection on the plane is taken, and its angle from the main
     * direction is returned. That angle is defined in the range
     * @f$ \left[ -\pi, \pi \right] @f$, so that it is 0 for a projection
     * matching the main direction and @f$ \pi/2 @f$ for one matching the
     * secondary direction.
     */
    double Angle(Vector_t const& v) const
    {
      double const a = std::atan2(VectorSecondaryComponent(v), VectorMainComponent(v));
      return (a >= M_PI) ? -M_PI : a;
    }

    /// @}

    /// @{
    /// @name Composition from plane to 3D

    /**
     * @brief Returns the 3D vector from the specified projection
     * @param projection the projection vector on the plane
     * @return the 3D vector representing the projection vector in world space
     * @see Projection()
     *
     * The returned vector is the 3D representation in space of the point of
     * the plane described by the specified projection.
     * The null projection is composed into a null vector.
     */
    Vector_t ComposeVector(Projection_t const& projection) const
    {
      return MainComponent(projection) * MainDir() +
             SecondaryComponent(projection) * SecondaryDir();
    }

    /**
     * @brief Returns the 3D point from the specified projection
     * @param projection the projection vector on the plane
     * @return the 3D point representing the projection vector in world space
     * @see Projection(), ReferencePoint()
     *
     * The returned point is the 3D representation in space of the point of
     * the plane described by the specified projection.
     * The null projection is composed into the reference point returned by
     * ReferencePoint().
     */
    Point_t ComposePoint(Projection_t const& projection) const
    {
      return ReferencePoint() + ComposeVector(projection);
    }

    /// @}

  private:
    AffinePlaneBase_t fPlaneBase; ///< Reference base.

  }; // class PlaneDecomposer<>

  /** **************************************************************************
   * @brief Class with methods to decompose and compose back vectors
   * @tparam Vector type to represent 3D vectors
   * @tparam Point type to represent 3D points
   * @tparam ProjVector type to represent 2D projection on plane
   *
   * These methods deal with projection of points and vectors on a plane
   * and the axis orthogonal to it.
   */
  template <typename Vector, typename Point, typename ProjVector>
  class Decomposer {

    using PlaneDecomposer_t = PlaneDecomposer<Vector, Point, ProjVector>;

    PlaneDecomposer_t fPlaneDecomp; ///< Manages the projection on the plane

    /// Returns the plane decomposer
    PlaneDecomposer_t const& Plane() const { return fPlaneDecomp; }

  public:
    /// Type for a point
    using Point_t = typename PlaneDecomposer_t::Point_t;

    ///< Type for a vector
    using Vector_t = typename PlaneDecomposer_t::Vector_t;

    /// Type representing the projection vector
    using Projection_t = typename PlaneDecomposer_t::Projection_t;

    /// Type representing the signed distance from the projection plane
    using Distance_t = typename PlaneDecomposer_t::Distance_t;

    /// Type representing a decomposition on the plane
    using DecomposedVector_t = typename PlaneDecomposer_t::DecomposedVector_t;

    /// Type of vector base for the space
    using AffinePlaneBase_t = typename PlaneDecomposer_t::AffinePlaneBase_t;

    /// Default constructor: projection on (x,y) with origin (0, 0, 0)
    Decomposer() = default;

    /// Constructor: specifies a base (an origin and two direction vectors)
    Decomposer(AffinePlaneBase_t&& base) : fPlaneDecomp(std::move(base)) {}

    /// Constructor: specifies a base (an origin and two direction vectors)
    Decomposer(AffinePlaneBase_t const& base) : fPlaneDecomp(base) {}

    /// @{
    /// @name Setters

    /// Change projection base
    void SetBase(AffinePlaneBase_t&& base) { fPlaneDecomp.SetBase(std::move(base)); }

    /// Change projection base
    void SetBase(AffinePlaneBase_t const& base) { fPlaneDecomp.SetBase(base); }

    /// Change the 3D point of the reference frame origin
    void SetOrigin(Point_t const& point) { fPlaneDecomp.SetOrigin(point); }

    /// Change the main direction of the projection base
    void SetMainDir(Vector_t const& dir) { fPlaneDecomp.SetMainDir(dir); }

    /// Change the secondary direction of the projection base
    void SetSecondaryDir(Vector_t const& dir) { fPlaneDecomp.SetSecondaryDir(dir); }

    /// @}

    /// @{
    /// @name Reference directions and point

    /// Returns the reference point for the plane coordinate, as a 3D point
    Point_t ReferencePoint() const { return Plane().ReferencePoint(); }

    /// Returns the base of the decomposition
    AffinePlaneBase_t const& Base() const { return Plane().Base(); }

    /// Returns the plane main axis direction
    Vector_t const& MainDir() const { return Plane().MainDir(); }

    /// Returns the plane secondary axis direction
    Vector_t const& SecondaryDir() const { return Plane().SecondaryDir(); }

    /// Returns the plane normal axis direction
    Vector_t const& NormalDir() const { return Base().NormalDir(); }

    /// @}

    /// @{
    /// @name Decomposition of a 3D point

    /// Returns the main component of a point
    auto PointMainComponent(Point_t const& point) const
    {
      return VectorMainComponent(Base().ToVector(point));
    }

    /// Returns the secondary component of a point
    auto PointSecondaryComponent(Point_t const& point) const
    {
      return VectorSecondaryComponent(Base().ToVector(point));
    }

    /// Returns the secondary component of a point
    auto PointNormalComponent(Point_t const& point) const
    {
      return VectorNormalComponent(Base().ToVector(point));
    }

    /**
     * @brief Returns the projection of the specified point on the plane
     * @param point the 3D point to be projected, in world coordinates
     * @return a 2D vector representing the projection of point on the plane
     *
     * The returned vector is a 2D vector expressing the projection of the point
     * (from world coordinates) on the plane.
     * The vector is expressed as @f$ ( m, s ) @f$, components following the
     * main direction (`MainDir()`) and the secondary one (`SecondaryDir()`)
     * respectively. The origin point is the one from ReferencePoint().
     */
    Projection_t ProjectPointOnPlane(Point_t const& point) const
    {
      return Plane().PointProjection(point);
    }

    /**
     * @brief Decomposes a 3D point in two components
     * @param point the point to be decomposed
     * @return the two components of point, on the plane and orthogonal to it
     *
     * The point is decomposed in:
     *
     * 1. a component orthogonal to the plane, expressed as a signed real number
     * 2. a component lying on the plane, expressed as a 2D vector
     *
     * The distance is obtained as by PointNormalComponent().
     * The projection on the plane is obtained following the same convention
     * as ProjectPointOnPlane().
     */
    DecomposedVector_t DecomposePoint(Point_t const& point) const
    {
      return DecomposeVector(Base().ToVector(point));
    }

    /// @}

    /// @{
    /// @name Decomposition of a 3D vector

    /// Returns the main component of a vector
    auto VectorMainComponent(Vector_t const& v) const { return Plane().VectorMainComponent(v); }

    /// Returns the secondary component of a vector
    auto VectorSecondaryComponent(Vector_t const& v) const
    {
      return Plane().VectorSecondaryComponent(v);
    }

    /// Returns the secondary component of a vector
    auto VectorNormalComponent(Vector_t const& v) const { return vect::dot(v, NormalDir()); }

    /**
     * @brief Returns the projection of the specified vector on the plane
     * @param v the 3D vector to be projected, in world units
     * @return a 2D vector representing the projection of v on the plane
     *
     * The returned vector is a 2D vector expressing the projection of the
     * vector (from world units) on the wire plane.
     * The vector is expressed as @f$ ( m, s ) @f$, components following the
     * main direction (`MainDir()`) and the secondary one (`SecondaryDir()`)
     * respectively.
     */
    Projection_t ProjectVectorOnPlane(Vector_t const& v) const
    {
      return Plane().VectorProjection(v);
    }

    /**
     * @brief Decomposes a 3D vector in two components
     * @param v the vector to be decomposed
     * @return the two components of vector, on the plane and orthogonal to it
     *
     * The vector is decomposed in:
     *
     * 1. a component orthogonal to the plane, expressed as a signed real number
     * 2. a component lying on the plane, expressed as a 2D vector
     *
     * The distance is obtained as by VectorNormalComponent().
     * The projection on the plane is obtained following the same convention
     * as ProjectVectorOnPlane().
     */
    DecomposedVector_t DecomposeVector(Vector_t const& v) const
    {
      return {VectorNormalComponent(v), ProjectVectorOnPlane(v)};
    }

    /**
     * @brief Returns the angle of the projection from main direction.
     * @param v vector to get the angle of
     * @return the angle of the projection from main direction, in radians
     *
     * The projection on the plane is taken, and its angle from the main
     * direction is returned. That angle is defined in the range
     * @f$ \left[ -\pi, \pi \right] @f$, so that it is 0 for a projection
     * matching the main direction and @f$ \pi/2 @f$ for one matching the
     * secondary direction.
     */
    double Angle(Vector_t const& v) const { return Plane().Angle(v); }

    /// @}

    /// @{
    /// @name Decomposition of a projection vector

    /// Returns the main component of a projection vector
    auto MainComponent(Projection_t const& v) const { return Plane().MainComponent(v); }

    /// Returns the secondary component of a projection vector
    auto SecondaryComponent(Projection_t const& v) const { return Plane().SecondaryComponent(v); }

    /// @}

    /// @{
    /// @name Composition of a point

    /**
     * @brief Returns the 3D point from composition of projection and distance
     * @param decomp decomposed point
     * @return the 3D point from composition of projection and distance
     * @see DecomposePoint(), ComposePoint(double, Projection_t const&)
     *
     * See `ComposePoint(double, Projection_t const&)` for details.
     */
    Point_t ComposePoint(DecomposedVector_t const& decomp) const
    {
      return ComposePoint(decomp.distance, decomp.projection);
    }

    /**
     * @brief Returns the 3D point from composition of projection and distance
     * @param distance distance of the target point from the wire plane
     * @param proj projection of the target point on the wire plane
     * @return the 3D point from composition of projection and distance
     * @see DecomposePoint()
     *
     * The returned point is the sum of two 3D contributions:
     *
     * 1. a vector parallel to the plane normal, with norm the input distance
     * 2. a vector lying on the plane, whose projection via
     *    `ProjectPointOnPlane()` gives the input projection
     *
     * Given the arbitrary definition of the projection reference, it is assumed
     * that the same convention is used as in ProjectPointOnPlane() and
     * PointNormalComponent().
     *
     */
    Point_t ComposePoint(double distance, Projection_t const& proj) const
    {
      return ReferencePoint() + ComposeVector(distance, proj);
    }

    /// @}

    /// @{
    /// @name Composition of a vector

    /**
     * @brief Returns the 3D vector from composition of projection and distance
     * @param decomp decomposed vector
     * @return the 3D vector from composition of projection and distance
     * @see DecomposeVector(), ComposeVector(double, Projection_t const&)
     *
     * See `ComposeVector(double, Projection_t const&)` for details.
     */
    Vector_t ComposeVector(DecomposedVector_t const& decomp) const
    {
      return ComposeVector(decomp.distance, decomp.projection);
    }

    /**
     * @brief Returns the 3D vector from composition of projection and distance
     * @param distance distance of the target point from the wire plane
     * @param proj projection of the target point on the wire plane
     * @return the 3D vector from composition of projection and distance
     * @see DecomposeVector()
     *
     * The returned vector is the sum of two 3D vectors:
     *
     * 1. a vector parallel to the plane normal, with norm the input distance
     * 2. a vector lying on the plane, whose projection via
     *    `ProjectVectorOnPlane()` gives the input projection
     *
     * Given the arbitrary definition of the projection reference, it is assumed
     * that the same convention is used as in ProjectVectorOnPlane() and
     * VectorNormalComponent().
     *
     */
    Vector_t ComposeVector(double distance, Projection_t const& proj) const
    {
      return Plane().ComposeVector(proj) + distance * NormalDir();
    }

    /// @}

  }; // class Decomposer<>

  /// @}
  // --- END Decomposition objects ---------------------------------------------

} // namespace geo

#endif // LARCOREALG_GEOMETRY_DECOMPOSER_H
