/**
 * @file   larcorealg/Geometry/BoxBoundedGeo.h
 * @brief  Provides a base class aware of world box coordinates
 * @see    larcorealg/Geometry/BoxBoundedGeo.cpp
 * @ingroup Geometry
 */

#ifndef LARCOREALG_GEOMETRY_BOXBOUNDEDGEO_H
#define LARCOREALG_GEOMETRY_BOXBOUNDEDGEO_H

// LArSoft libraries
#include "larcoreobj/SimpleTypesAndConstants/geo_vectors.h" // geo::vect

// ROOT library
//#include "TVector3.h"

// C/C++ standard library
#include <algorithm>
#include <vector>

namespace geo {
  /**
   * @brief A base class aware of world box coordinates
   * @ingroup Geometry
   *
   * An object describing a simple shape can inherit from this one to gain
   * access to a common boundary checking interface.
   *
   * The boundary is a simple box with axes parallel to the coordinate system.
   */
  class BoxBoundedGeo {
  public:
    using Coords_t = Point_t;         ///< Type of the coordinate triplet.
    using Coord_t = Coords_t::Scalar; ///< Type of the coordinate.

    /**
     * @brief Default constructor: sets an empty volume
     * @see SetBoundaries
     *
     * We allow for a default construction since the derived object might need
     * some specific action before being aware of its boundaries.
     * In that case, SetBoundaries() will set the boundaries.
     */
    BoxBoundedGeo() = default;

    /**
     * @brief Constructor: sets the boundaries in world coordinates as specified
     * @param x_min lower x coordinate
     * @param x_max upper x coordinate
     * @param y_min lower y coordinate
     * @param y_max upper y coordinate
     * @param z_min lower z coordinate
     * @param z_max upper z coordinate
     * @see `SetBoundaries()`
     *
     * Note that is assumed that each minimum is larger than its maximum,
     * and no check is performed.
     */
    BoxBoundedGeo(Coord_t x_min,
                  Coord_t x_max,
                  Coord_t y_min,
                  Coord_t y_max,
                  Coord_t z_min,
                  Coord_t z_max)
      : c_min{x_min, y_min, z_min}, c_max{x_max, y_max, z_max}
    {
      SortCoordinates();
    }

    /**
     * @brief Constructor: sets the boundaries in world coordinates as specified
     * @param lower lower coordinates (x, y, z)
     * @param upper upper coordinates (x, y, z)
     * @see `SetBoundaries()`
     *
     * Note that is assumed that each minimum is larger than its maximum,
     * and no check is performed.
     */
    BoxBoundedGeo(Coords_t lower, Coords_t upper) : c_min(lower), c_max(upper)
    {
      SortCoordinates();
    }

    /// @{
    /// @name Dimension queries

    /// Returns the world x coordinate of the start of the box.
    double MinX() const { return c_min.X(); }

    /// Returns the world x coordinate of the end of the box.
    double MaxX() const { return c_max.X(); }

    /// Returns the world x coordinate of the center of the box.
    double CenterX() const { return (MinX() + MaxX()) / 2.; }

    /// Returns the full size in the X dimension.
    double SizeX() const { return MaxX() - MinX(); }

    /// Returns the size from the center to the border on X dimension.
    double HalfSizeX() const { return SizeX() / 2.0; }

    /// Returns the world y coordinate of the start of the box.
    double MinY() const { return c_min.Y(); }

    /// Returns the world y coordinate of the end of the box.
    double MaxY() const { return c_max.Y(); }

    /// Returns the world y coordinate of the center of the box.
    double CenterY() const { return (MinY() + MaxY()) / 2.; }

    /// Returns the full size in the Y dimension.
    double SizeY() const { return MaxY() - MinY(); }

    /// Returns the size from the center to the border on Y dimension.
    double HalfSizeY() const { return SizeY() / 2.0; }

    /// Returns the world z coordinate of the start of the box.
    double MinZ() const { return c_min.Z(); }

    /// Returns the world z coordinate of the end of the box.
    double MaxZ() const { return c_max.Z(); }

    /// Returns the world z coordinate of the center of the box.
    double CenterZ() const { return (MinZ() + MaxZ()) / 2.; }

    /// Returns the full size in the Z dimension.
    double SizeZ() const { return MaxZ() - MinZ(); }

    /// Returns the size from the center to the border on Z dimension.
    double HalfSizeZ() const { return SizeZ() / 2.0; }

    /// Returns the corner point with the smallest coordinates.
    Point_t Min() const { return c_min; }

    /// Returns the corner point with the largest coordinates.
    Point_t Max() const { return c_max; }

    /// Returns the center point of the box.
    Point_t Center() const { return {CenterX(), CenterY(), CenterZ()}; }

    /// @}

    /// @name Containment in the full volume
    /// @{

    /**
     * @brief Returns whether this TPC contains the specified world x coordinate
     * @param x the absolute ("world") coordinate x
     * @param wiggle expansion factor for the range (see ContainsPosition())
     * @return whether the specified coordinate is in this TPC
     * @see ContainsPosition()
     *
     * Note that x is by definition the drift direction, and a reconstructed x
     * typically depends on an assumption respect to the event time.
     */
    bool ContainsX(double x, double const wiggle = 1) const
    {
      return CoordinateContained(x, MinX(), MaxX(), wiggle);
    }

    /**
     * @brief Returns whether this TPC contains the specified world y coordinate
     * @param y the absolute ("world") coordinate y
     * @param wiggle expansion factor for the range (see ContainsPosition())
     * @return whether the specified coordinate is in this TPC
     * @see ContainsPosition()
     */
    bool ContainsY(double y, double const wiggle = 1) const
    {
      return CoordinateContained(y, MinY(), MaxY(), wiggle);
    }

    /**
     * @brief Returns whether this TPC contains the specified world z coordinate
     * @param z the absolute ("world") coordinate z
     * @param wiggle expansion factor for the range (see ContainsPosition())
     * @return whether the specified coordinate is in this TPC
     * @see ContainsPosition()
     */
    bool ContainsZ(double z, double const wiggle = 1) const
    {
      return CoordinateContained(z, MinZ(), MaxZ(), wiggle);
    }

    /**
     * @brief Returns if TPC contains the specified world y and z coordinates
     * @param y the absolute ("world") coordinate y
     * @param z the absolute ("world") coordinate z
     * @param wiggle expansion factor for the range (see ContainsPosition())
     * @return whether the specified coordinate is in this TPC
     * @see ContainsPosition()
     */
    bool ContainsYZ(double y, double z, double const wiggle = 1) const
    {
      return ContainsY(y, wiggle) && ContainsZ(z, wiggle);
    }

    /**
     * @brief Returns whether this volume contains the specified point.
     * @param point the point [cm]
     * @param wiggle expansion factor for the range
     * @return whether the specified coordinate is in this volume
     *
     * If the wiggle is larger than 1, each size of the volume is expanded by
     * the wiggle factor.
     * If the wiggle is less than 1, each size is shrinked.
     */
    bool ContainsPosition(Point_t const& point, double wiggle = 1.0) const
    {
      return ContainsX(point.X(), wiggle) && ContainsYZ(point.Y(), point.Z(), wiggle);
    } // ContainsPosition()
    /// @see `ContainsPosition(Point_t const&, double) const`.
    bool ContainsPosition(TVector3 const& point, double wiggle = 1.0) const;
    /// @see `ContainsPosition(Point_t const&, double) const`.
    bool ContainsPosition(double const* point, double wiggle = 1.0) const;

    /// @}

    /// @name Containment in a fiducial volume
    /// @{
    /**
     * @brief Returns whether TPC fiducial volume contains world x coordinate.
     * @param x the absolute ("world") coordinate x
     * @param neg_margin how far within the TPC the fiducial region starts
     * @param pos_margin how far before the TPC the fiducial region ends
     * @return whether the specified coordinate is in this TPC fiducial volume
     *
     * The fiducial volume is defined by the specified margins, that denote how
     * much of the coordinate is not fiducial. For example,
     *
     *     bool bWithin = tpc->InFiducialX(x, 5., 8.);
     *
     * on a TPC with x from 0 to 100 (cm, presumably) will return true for all
     * `x` between 5 and 92.
     * If positive margin is not specified, it's assumed to be the same as the
     * negative one.
     * Specifying a negative mergin effectively extends the TPC volume.
     * Note that x is by definition the drift direction, and a reconstructed x
     * typically depends on an assumption respect to the event time.
     */
    bool InFiducialX(double x, double neg_margin, double pos_margin) const
    {
      return CoordinateContained(x, MinX() + neg_margin, MaxX() - pos_margin);
    }
    /**
     * @brief Returns whether TPC fiducial volume contains world x coordinate.
     * @see `InFiducialX(double, double, double) const`
     *
     * Margins are symmetric.
     */
    bool InFiducialX(double x, double margin) const { return InFiducialX(x, margin, margin); }

    /**
     * @brief Returns whether TPC fiducial volume contains world y coordinate
     * @param y the absolute ("world") coordinate y
     * @param neg_margin how far within the TPC the fiducial region starts
     * @param pos_margin how far before the TPC the fiducial region ends
     * @return whether the specified coordinate is in this TPC fiducial volume
     *
     * The fiducial volume is defined by the specified margins, that denote how
     * much of the coordinate is not fiducial. For example,
     *
     *     bool bWithin = tpc->InFiducialY(y, 5., 8.);
     *
     * on a TPC with y from 0 to 100 (cm, presumably) will return true for all
     * `y` between 5 and 92.
     * If positive margin is not specified, it's assumed to be the same as the
     * negative one.
     * Specifying a negative mergin effectively extends the TPC volume.
     */
    bool InFiducialY(double y, double neg_margin, double pos_margin) const
    {
      return CoordinateContained(y, MinY() + neg_margin, MaxY() - pos_margin);
    }
    /**
     * @brief Returns whether TPC fiducial volume contains world y coordinate.
     * @see `InFiducialY(double, double, double) const`
     *
     * Margins are symmetric.
     */
    bool InFiducialY(double y, double margin) const { return InFiducialY(y, margin, margin); }

    /**
     * @brief Returns whether TPC fiducial volume contains world z coordinate
     * @param z the absolute ("world") coordinate y
     * @param neg_margin how far within the TPC the fiducial region starts
     * @param pos_margin how far before the TPC the fiducial region ends
     * @return whether the specified coordinate is in this TPC fiducial volume
     *
     * The fiducial volume is defined by the specified margins, that denote how
     * much of the coordinate is not fiducial. For example,
     *
     *     bool bWithin = tpc->InFiducialZ(z, 5., 8.);
     *
     * on a TPC with z from 0 to 100 (cm, presumably) will return true for all
     * `z` between 5 and 92.
     * If positive margin is not specified, it's assumed to be the same as the
     * negative one.
     * Specifying a negative mergin effectively extends the TPC volume.
     */
    bool InFiducialZ(double z, double neg_margin, double pos_margin) const
    {
      return CoordinateContained(z, MinZ() + neg_margin, MaxZ() - pos_margin);
    }
    /**
     * @brief Returns whether TPC fiducial volume contains world z coordinate.
     * @see `InFiducialZ(double, double, double) const`
     *
     * Margins are symmetric.
     */
    bool InFiducialZ(double z, double margin) const { return InFiducialZ(z, margin, margin); }

    /// @}

    // -- BEGIN -- Overlaps ----------------------------------------------------
    /// @name Overlaps
    /// @{

    /// Returns if the _x_ coordinates covered by this and `other` box overlap.
    bool OverlapsX(BoxBoundedGeo const& other) const
    {
      return std::min(MaxX(), other.MaxX()) > std::max(MinX(), other.MinX());
    }

    /// Returns if the _y_ coordinates covered by this and `other` box overlap.
    bool OverlapsY(BoxBoundedGeo const& other) const
    {
      return std::min(MaxY(), other.MaxY()) > std::max(MinY(), other.MinY());
    }

    /// Returns if the _z_ coordinates covered by this and `other` box overlap.
    bool OverlapsZ(BoxBoundedGeo const& other) const
    {
      return std::min(MaxZ(), other.MaxZ()) > std::max(MinZ(), other.MinZ());
    }

    /// Returns if this and `other` box overlap.
    bool Overlaps(BoxBoundedGeo const& other) const
    {
      return OverlapsX(other) && OverlapsY(other) && OverlapsZ(other);
    }

    /// @}
    // -- END -- Overlaps ------------------------------------------------------

    /**
     * @brief Returns whether the specified coordinate is in a range
     * @param c the coordinate
     * @param min lower boundary of the range
     * @param max upper boundary of the range
     * @param wiggle expansion factor for the range
     * @return whether the specified coordinate is in a range
     *
     * If the wiggle is larger than 1, the range is expanded by the wiggle factor.
     * If the wiggle is less than 1, the range is shrinked.
     */
    static bool CoordinateContained(double c, double min, double max, double wiggle = 1.)
    {
      return (c >= (min > 0 ? min / wiggle : min * wiggle)) &&
             (c <= (max < 0 ? max / wiggle : max * wiggle));
    } // CoordinateContained()

    /**
     * @brief Returns whether the specified coordinate is in a range
     * @param c the coordinate
     * @param range pointer to [ lower boundary, upper boundary ] of the range
     * @param wiggle expansion factor for the range
     * @return whether the specified coordinate is in a range
     * @see `CoordinateContained(double, double, double, double)`
     *
     * If the wiggle is larger than 1, the range is expanded by the wiggle factor.
     * If the wiggle is less than 1, the range is shrinked.
     */
    static bool CoordinateContained(double c, double const* range, double wiggle = 1.)
    {
      return CoordinateContained(c, range[0], range[1], wiggle);
    }

    /// @{
    /// @name Setting dimensions

    /**
     * @brief Sets the boundaries in world coordinates as specified.
     * @param x_min lower x coordinate
     * @param x_max upper x coordinate
     * @param y_min lower y coordinate
     * @param y_max upper y coordinate
     * @param z_min lower z coordinate
     * @param z_max upper z coordinate
     */
    void SetBoundaries(Coord_t x_min,
                       Coord_t x_max,
                       Coord_t y_min,
                       Coord_t y_max,
                       Coord_t z_min,
                       Coord_t z_max)
    {
      c_min.SetXYZ(x_min, y_min, z_min);
      c_max.SetXYZ(x_max, y_max, z_max);
      SortCoordinates();
    }

    /**
     * @brief Sets the boundaries in world coordinates as specified.
     * @param lower lower coordinates (x, y, z)
     * @param upper upper coordinates (x, y, z)
     */
    void SetBoundaries(Coords_t lower, Coords_t upper)
    {
      c_min = lower;
      c_max = upper;
      SortCoordinates();
    }

    /**
     * @brief Extends the current box to also include the specified point.
     * @param x x coordinate of the point to include
     * @param y y coordinate of the point to include
     * @param z z coordinate of the point to include
     */
    void ExtendToInclude(Coord_t x, Coord_t y, Coord_t z) { ExtendToInclude(Point_t(x, y, z)); }

    /**
     * @brief Extends the current box to also include the specified point.
     * @param point coordinates of the point to include
     */
    void ExtendToInclude(Point_t const& point)
    {
      set_min(c_min, point);
      set_max(c_max, point);
    }

    /**
     * @brief Extends the current box to also include the specified one
     * @param box the box to include
     *
     * It is assumed that the box has its boundaries properly sorted.
     */
    void ExtendToInclude(BoxBoundedGeo const& box)
    {
      set_min(c_min, box.Min());
      set_max(c_max, box.Max());
    } // ExtendToInclude()

    /// @}

    //@{
    /**
     * @brief Calculates the entry and exit points of a trajectory on the box surface
     * @param TrajectoryStart position of the trajectory source
     * @param TrajectoryDirect direction vector of the trajectory
     *
     * This member is public since it just gives an output and does not change any member.
     * The algorithm works only for a box shaped active volume with facing walls parallel to axis.
     * If the return std::vector is empty the trajectory does not intersect with the box.
     * Normally the return value should have one (if the trajectory originates in the box) or two (else) entries.
     * If the return value has two entries the first represents the entry point and the second the exit point
     */
    std::vector<TVector3> GetIntersections(TVector3 const& TrajectoryStart,
                                           TVector3 const& TrajectoryDirect) const;
    std::vector<Point_t> GetIntersections(Point_t const& TrajectoryStart,
                                          Vector_t const& TrajectoryDirect) const;
    //@}

    /// Sets var to value if value is smaller than the current var value.
    static void set_min(Coord_t& var, Coord_t value)
    {
      if (value < var) var = value;
    }

    /// Sets var to value if value is larger than the current var value.
    static void set_max(Coord_t& var, Coord_t value)
    {
      if (value > var) var = value;
    }

    /// Sets each coordinate of var to the one in value if the latter is smaller.
    static void set_min(Coords_t& var, Point_t const& value)
    {
      if (value.X() < var.X()) var.SetX(value.X());
      if (value.Y() < var.Y()) var.SetY(value.Y());
      if (value.Z() < var.Z()) var.SetZ(value.Z());
    }

    /// Sets each coordinate of var to the one in value if the latter is larger.
    static void set_max(Coords_t& var, Point_t const& value)
    {
      if (value.X() > var.X()) var.SetX(value.X());
      if (value.Y() > var.Y()) var.SetY(value.Y());
      if (value.Z() > var.Z()) var.SetZ(value.Z());
    }

  private:
    // we don't allow the derived classes to mess with the boundaries
    Coords_t c_min; ///< minimum coordinates (x, y, z)
    Coords_t c_max; ///< maximum coordinates (x, y, z)

    /// Makes sure each coordinate of the minimum point is smaller than maximum.
    void SortCoordinates();

  }; // class BoxBoundedGeo

} // namespace geo

#endif // LARCOREALG_GEOMETRY_BOXBOUNDEDGEO_H
