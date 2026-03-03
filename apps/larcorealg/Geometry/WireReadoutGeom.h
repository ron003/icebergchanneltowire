#ifndef LARCOREALG_GEOMETRY_WIREREADOUTGEOM_H
#define LARCOREALG_GEOMETRY_WIREREADOUTGEOM_H
// vim: sw=2 expandtab :

////////////////////////////////////////////////////////////////////////
/// @file  larcorealg/Geometry/WireReadoutGeom.h
/// @brief Interface to geometry for wire readouts
/// @ingroup Geometry
////////////////////////////////////////////////////////////////////////

// LArSoft libraries
#include "larcorealg/Geometry/Iterable.h"
#include "larcorealg/Geometry/PlaneGeo.h"
#include "larcorealg/Geometry/WireGeo.h"
#include "larcorealg/Geometry/WireReadoutGeometryBuilder.h"
#include "larcorealg/Geometry/WireReadoutSorter.h"
#include "larcorealg/Geometry/details/ReadoutIterationPolicy.h"
#include "larcorealg/Geometry/details/ToGeometryElement.h"
#include "larcorealg/Geometry/details/ZeroIDs.h"
#include "larcorealg/Geometry/fwd.h"
#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h" // raw::ChannelID_t
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_vectors.h"
#include "larcoreobj/SimpleTypesAndConstants/readout_types.h"

// C/C++ standard libraries
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace geo {

  /**
   * @brief Interface for a class providing readout channel mapping to geometry
   * @ingroup Geometry
   *
   * @note A number of methods react specifically when provided with invalid IDs as
   * arguments. An invalid ID is an ID with the `isValid` flag unset, or, in case of
   * channel IDs, an ID with value `raw::InvalidChannelID`.  An ID that does not present
   * this feature is by definition "valid"; this does not imply that the represented
   * entity (channel, geometry entity or readout group) actually exists. *The behaviour of
   * the methods to valid, non-existent IDs should be considered undefined*, and it is
   * recommended that the existence of the entity is checked beforehand.  Unless the
   * documentation explicitly defines a behaviour, an undefined behaviour should be
   * assumed; nevertheless, the documentation of some of the methods still reminds of
   * this.
   */
  class WireReadoutGeom
    : Iterable<details::ReadoutIterationPolicy, details::ToReadoutGeometryElement> {
    using Iteration = Iterable<details::ReadoutIterationPolicy, details::ToReadoutGeometryElement>;

  public:
    explicit WireReadoutGeom(GeometryCore const* geom,
                             std::unique_ptr<WireReadoutGeometryBuilder> builder,
                             std::unique_ptr<WireReadoutSorter> sorter);
    virtual ~WireReadoutGeom();

    using Segment = std::pair<Point_t, Point_t>;
    static auto const& start(Segment const& s) { return s.first; }
    static auto const& finish(Segment const& s) { return s.second; }

    //--------------------------------------------------------------------------
    // Iteration facilities
    using Iteration::begin;
    using Iteration::end;
    using Iteration::Iterate;

    //@{
    /**
     * @brief Returns whether we have the specified plane
     *
     * The HasElement() method is overloaded and its meaning depends on the type of ID.
     */
    bool HasPlane(PlaneID const& planeid) const;
    bool HasElement(PlaneID const& planeid) const { return HasPlane(planeid); }
    //@}

    /// @} Plane access and information

    //
    // group features
    //

    /// Returns the largest number of planes among all TPCs in this detector
    unsigned int MaxPlanes() const;

    //@{
    /**
     * @brief Returns the total number of planes in the specified TPC
     * @param tpcid TPC ID
     * @return number of planes in specified TPC, or 0 if no TPC found
     *
     * The NElements() and NSiblingElements() methods are overloaded and their return
     * depends on the type of ID.
     *
     * @todo Change return type to size_t
     */
    unsigned int Nplanes(TPCID const& tpcid = details::tpc_zero) const;
    unsigned int NElements(TPCID const& tpcid) const { return Nplanes(tpcid); }
    unsigned int NSiblingElements(PlaneID const& planeid) const { return Nplanes(planeid); }
    //@}

    /**
     * @brief Returns the number of views (different wire orientations)
     *
     * Returns the number of different views, or wire orientations, in the detector.
     *
     * The function assumes that all TPCs in all cryostats of a detector have the same
     * number of planes, which should be a safe assumption.
     *
     * @todo Change return type to size_t
     */
    unsigned int Nviews() const;

    /**
     * @brief Returns a list of possible views in the detector.
     * @return the set of views
     */
    std::set<View_t> const& Views() const { return allViews; }

    std::set<View_t> Views(TPCID const& tpcid) const;

    //@{
    /**
     * @brief Returns the first plane of the specified TPC
     * @param tpc ID of the TPC
     * @return a constant reference to the specified plane
     * @throw cet::exception (`WireReadoutGeom` category) if plane not present
     */
    PlaneGeo const& FirstPlane(TPCID const& tpcid) const;
    //@}

    //@{
    /**
     * @brief Returns the specified wire
     * @param tpcid ID of the TPC
     * @param view number of the requested view
     * @param planeid ID of the plane
     * @return a constant reference to the specified plane
     * @throw cet::exception (`GeometryCore` category) if cryostat not present
     * @throw cet::exception (`TPCOutOfRange` category) if no such TPC
     * @throw cet::exception (`PlaneOutOfRange` category) if no such plane
     *
     * The GetElement() method is overloaded and its return depends on the type of ID.
     */
    PlaneGeo const& Plane(TPCID const& tpcid, View_t view) const;
    PlaneGeo const& Plane(PlaneID const& planeid) const;
    PlaneGeo const& GetElement(PlaneID const& planeid) const { return Plane(planeid); }
    //@}

    //@{
    /**
     * @brief Returns the specified plane
     * @param planeid plane ID
     * @return a constant pointer to the specified plane, or nullptr if none
     *
     * The GetElementPtr() method is overloaded and its return depends on the type of ID.
     */
    PlaneGeo const* PlanePtr(PlaneID const& planeid) const;
    PlaneGeo const* GetElementPtr(PlaneID const& planeid) const { return PlanePtr(planeid); }
    //@}

    //--------------------------------------------------------------------------
    /// @{
    /// @name Wire details

    unsigned int MaxWires() const;

    //@{
    /**
     * @brief Returns the total number of wires in the specified plane
     * @param planeid plane ID
     * @return number of wires in specified plane, or 0 if no plane found
     *
     * The NElements() and NSiblingElements() methods are overloaded and their return
     * depends on the type of ID.
     *
     * @todo Change return type to size_t
     */
    unsigned int Nwires(PlaneID const& planeid) const;
    unsigned int NElements(PlaneID const& planeid) const;
    unsigned int NSiblingElements(WireID const& wireid) const { return Nwires(wireid); }

    //@}

    //
    // access
    //

    //@{
    /**
     * @brief Returns whether we have the specified wire
     *
     * The HasElement() method is overloaded and its meaning depends on the type of ID.
     */
    bool HasWire(WireID const& wireid) const;
    bool HasElement(WireID const& wireid) const { return HasWire(wireid); }
    //@}

    //@{
    /**
     * @brief Returns the specified wire
     * @param wireid wire ID
     * @return a constant pointer to the specified wire, or nullptr if none
     *
     * The GetElementPtr() method is overloaded and its return depends on the type of ID.
     */
    WireGeo const* WirePtr(WireID const& wireid) const;
    WireGeo const* GetElementPtr(WireID const& wireid) const { return WirePtr(wireid); }
    //@}

    //@{
    /**
     * @brief Returns the specified wire
     * @param wireid ID of the wire
     * @return a constant reference to the specified wire
     * @throw cet::exception if not found
     *
     * The GetElement() method is overloaded and its return depends on the type of ID.
     */
    WireGeo const& Wire(WireID const& wireid) const;
    WireGeo const& GetElement(WireID const& wireid) const { return Wire(wireid); }
    //@}

    //
    // simple geometry queries
    //

    /**
     * @brief Fills two arrays with the coordinates of the wire end points
     * @param wireid ID of the wire
     * @param xyzStart (output) an array with the start coordinate
     * @param xyzEnd (output) an array with the end coordinate
     * @throws cet::exception wire not present
     *
     * The starting point is the wire end with lower z coordinate.
     *
     * @deprecated use the wire ID interface instead (but note that it does not sort the
     *             ends)
     */
    void WireEndPoints(WireID const& wireid, double* xyzStart, double* xyzEnd) const;

    //@{
    /**
     * @brief Returns a segment whose ends are the wire end points
     * @param wireid ID of the wire
     * @return a segment whose ends are the wire end points
     * @throws cet::exception wire not present
     *
     * The start and end are assigned as returned from the geo::WireGeo object.  The rules
     * for this assignment are documented in that class.
     *
     * @deprecated use the wire ID interface instead (but note that it does not sort the
     *             ends)
     */
    Segment WireEndPoints(WireID const& wireid) const
    {
      WireGeo const& wire = Wire(wireid);
      return {wire.GetStart(), wire.GetEnd()};
    }
    //@}

    /// @name Wire access and information
    /// @{

    //
    // single object features
    //

    //@{
    /**
     * @brief Returns the angle of the wires in the specified view from vertical
     * @param view the view
     * @param tpcid ID of the TPC
     * @return the angle [radians]
     * @throw cet::exception ("GeometryCore" category) if no such view
     *
     * The angle is defined as in WireGeo::ThetaZ().
     *
     * This method assumes all wires in the view have the same angle (it queries for the
     * first).
     *
     * @deprecated This does not feel APA-ready
     */
    double WireAngleToVertical(View_t view, TPCID const& tpcid) const;
    //@}

    /// @} Wire access and information

    //
    // wire intersections
    //

    //@{
    /**
     * @brief Computes the intersection between two wires.
     * @param wid1 ID of the first wire
     * @param wid2 ID of the other wire
     * @param[out] intersection the intersection point (global coordinates)
     * @return whether an intersection was found inside the TPC the wires belong
     * @see `geo::WiresIntersection()`, `geo::LineClosestPoint()`
     *
     * The wires identified by `wid1` and `wid2` are intersected, and the 3D intersection
     * point is written into the `intersection` parameter.  The "intersection" point is
     * actually the point belonging to the first wire (`wid2`) which is the closest (in
     * Euclidean 3D metric) to the second wire.
     *
     * The intersection is computed only if the wires belong to different planes of the
     * same TPC. If that is not the case (i.e. they belong to different TPC or cryostat,
     * or if they belong to the same plane), `false` is returned and `intersection` is set
     * with all components to infinity (`std::numeric_limits<>::infinity()`).
     *
     * When the intersection is computed, it is always stored in the `intersection` output
     * parameter. Return value is `true` if this intersection lies within the physical
     * boundaries first wire, while it is instead `false` if it lies on the extrapolation
     * of the wire direction, but not within the wire physical extension.
     *
     * To test that the result is not infinity (nor NaN), use
     * `geo::vect::isfinite(intersection)` etc.
     *
     * @note If `geo::WireGeo` objects are already available, using instead the free
     *       function `geo::WiresIntersection()` or the method
     *       `geo::WireGeo::IntersectionWith()` is faster (and _recommended_).  For purely
     *       geometric intersection, `geo::LineClosestPoint()` is also available.
     */
    bool WireIDsIntersect(WireID const& wid1, WireID const& wid2, Point_t& intersection) const;
    //@}

    //@{
    /**
     * @brief Computes the intersection between two wires.
     * @param wid1 ID of the first wire
     * @param wid2 ID of the other wire
     * @return whether an intersection was found within the TPC
     *
     * The "intersection" refers to the projection of the wires into the same @f$ x = 0
     * @f$ plane.  Wires are assumed to have at most one intersection.  If wires are
     * parallel, `widIntersect` will have the two components set to infinity
     * (`std::numeric_limits<>::infinity()`) and the TPC number set to invalid
     * (`geo::TPCID::InvalidID`). Also, `false` is returned.  If the intersection is
     * outside the TPC, `false` is also returned, but the `widIntersect` will contain the
     * coordinates of that intersection. The TPC number is still set to invalid, although
     * the intersection _might_ belong to a valid TPC somewhere else.
     *
     *
     * @deprecated This method uses arbitrary assumptions and should not be used. Use the
     *             interface returning a full vector instead.
     */
    std::optional<WireIDIntersection> WireIDsIntersect(WireID const& wid1,
                                                       WireID const& wid2) const;
    //@}

    /// @}

    /// @name Plane access and information
    /// @{

    //
    // access
    //

    /**
     * @name Wire geometry queries
     *
     * Please note the differences between functions:
     * ChannelsIntersect(), WireIDsIntersect() and IntersectionPoint()
     * all calculate wires intersection using the same equation.
     * ChannelsIntersect() and WireIdsIntersect() will return true
     * if the two wires cross, return false if they don't.
     * IntersectionPoint() does not check if the two wires cross.
     */
    /// @{

    /**
     * @brief Returns the plane that is not in the specified arguments
     * @param pid1 a plane
     * @param pid2 another plane
     * @return the ID to the third plane
     * @throws cet::exception (category: "GeometryCore") if other than 3 planes
     * @throws cet::exception (category: "GeometryCore") if pid1 and pid2 match
     *
     * This function requires a geometry with exactly three planes.  If the two input
     * planes are not on the same TPC, the result is undefined.
     */
    PlaneID ThirdPlane(PlaneID const& pid1, PlaneID const& pid2) const;

    /**
     * @brief Returns the slope on the third plane, given it in the other two
     * @param pid1 ID of the plane of the first slope
     * @param slope1 slope as seen on the first plane
     * @param pid2 ID of the plane of the second slope
     * @param slope2 slope as seen on the second plane
     * @param output_plane ID of the plane on which to calculate the slope
     * @return the slope on the third plane, or -999. if slope would be infinity
     * @throws cet::exception (category: "GeometryCore") if different TPC
     * @throws cet::exception (category: "GeometryCore") if input planes match
     *
     * Given a slope as projected in two planes, returns the slope as projected in the
     * specified output plane.  The slopes are defined in uniform units; they should be
     * computed as distance ratios (or tangent of a geometrical angle; the formula is
     * still valid using dt/dw directly in case of equal wire pitch in all planes and
     * uniform drift velocity.
     */
    double ThirdPlaneSlope(PlaneID const& pid1,
                           double slope1,
                           PlaneID const& pid2,
                           double slope2,
                           PlaneID const& output_plane) const;

    /**
     * @brief Returns the slope on the third plane, given it in the other two
     * @param pid1 ID of the plane of the first slope
     * @param slope1 slope as seen on the first plane
     * @param pid2 ID of the plane of the second slope
     * @param slope2 slope as seen on the second plane
     * @return the slope on the third plane, or -999. if slope would be infinity
     * @throws cet::exception (category: "GeometryCore") if different TPC
     * @throws cet::exception (category: "GeometryCore") if same plane
     * @throws cet::exception (category: "GeometryCore") if other than 3 planes
     *
     * Given a slope as projected in two planes, returns the slope as projected in the
     * third plane.  This function is a shortcut assuming exactly three wire planes in the
     * TPC, in which case the output plane is chosen as the one that is neither of the
     * input planes.
     */
    double ThirdPlaneSlope(PlaneID const& pid1,
                           double slope1,
                           PlaneID const& pid2,
                           double slope2) const;

    //@{
    /**
     * @brief Returns the slope on the third plane, given it in the other two
     * @param plane1 index of the plane of the first slope
     * @param slope1 slope as seen on the first plane
     * @param plane2 index of the plane of the second slope
     * @param slope2 slope as seen on the second plane
     * @param tpcid TPC where the two planes belong
     * @return the slope on the third plane, or -999. if slope would be infinity
     * @throws cet::exception (category: "GeometryCore") if different TPC
     * @throws cet::exception (category: "GeometryCore") if same plane
     * @throws cet::exception (category: "GeometryCore") if other than 3 planes
     *
     * Given a slope as projected in two planes, returns the slope as projected in the
     * third plane.
     */
    double ThirdPlaneSlope(PlaneID::PlaneID_t plane1,
                           double slope1,
                           PlaneID::PlaneID_t plane2,
                           double slope2,
                           TPCID const& tpcid) const
    {
      return ThirdPlaneSlope(PlaneID(tpcid, plane1), slope1, PlaneID(tpcid, plane2), slope2);
    }
    //@}

    /**
     * @brief Returns dT/dW on the third plane, given it in the other two
     * @param pid1 ID of the plane of the first dT/dW
     * @param dTdW1 dT/dW as seen on the first plane
     * @param pid2 ID of the plane of the second dT/dW
     * @param dTdW2 dT/dW  as seen on the second plane
     * @param output_plane ID of the plane on which to calculate the slope
     * @return dT/dW on the third plane, or -999. if dT/dW would be infinity
     * @throws cet::exception (category: "GeometryCore") if different TPC
     * @throws cet::exception (category: "GeometryCore") if same plane
     * @throws cet::exception (category: "GeometryCore") if other than 3 planes
     *
     * Given a dT/dW as projected in two planes, returns the dT/dW as projected in the
     * third plane.  The dT/dW are defined in time ticks/wide number units.
     */
    double ThirdPlane_dTdW(PlaneID const& pid1,
                           double slope1,
                           PlaneID const& pid2,
                           double slope2,
                           PlaneID const& output_plane) const;

    /**
     * @brief Returns dT/dW on the third plane, given it in the other two
     * @param pid1 ID of the plane of the first dT/dW
     * @param dTdW1 dT/dW as seen on the first plane
     * @param pid2 ID of the plane of the second dT/dW
     * @param dTdW2 dT/dW  as seen on the second plane
     * @return dT/dW on the third plane, or -999. if dT/dW would be infinity
     * @throws cet::exception (category: "GeometryCore") if different TPC
     * @throws cet::exception (category: "GeometryCore") if same plane
     * @throws cet::exception (category: "GeometryCore") if other than 3 planes
     *
     * Given a dT/dW as projected in two planes, returns the dT/dW as projected in the
     * third plane.  This function is a shortcut assuming exactly three wire planes in the
     * TPC, in which case the output plane is chosen as the one that is neither of the
     * input planes.
     */
    double ThirdPlane_dTdW(PlaneID const& pid1,
                           double slope1,
                           PlaneID const& pid2,
                           double slope2) const;

    /**
     * @brief Returns the slope on the third plane, given it in the other two
     * @param angle1 angle or the wires on the first plane
     * @param slope1 slope as observed on the first plane
     * @param angle2 angle or the wires on the second plane
     * @param slope2 slope as observed on the second plane
     * @param angle_target angle or the wires on the target plane
     * @return the slope as measure on the third plane, or 999 if infinity
     *
     * This function will return a small slope if both input slopes are small.
     */
    static double ComputeThirdPlaneSlope(double angle1,
                                         double slope1,
                                         double angle2,
                                         double slope2,
                                         double angle_target);

    /**
     * @brief Returns the slope on the third plane, given it in the other two
     * @param angle1 angle or the wires on the first plane
     * @param pitch1 wire pitch on the first plane
     * @param dTdW1 slope in dt/dw units as observed on the first plane
     * @param angle2 angle or the wires on the second plane
     * @param pitch2 wire pitch on the second plane
     * @param dTdW2 slope in dt/dw units as observed on the second plane
     * @param angle_target angle or the wires on the target plane
     * @param pitch_target wire pitch on the target plane
     * @return dt/dw slope as measured on the third plane, or 999 if infinity
     *
     * The input slope must be specified in dt/dw non-homogeneous coordinates.
     *
     * This function will return a small slope if both input slopes are small.
     */
    static double ComputeThirdPlane_dTdW(double angle1,
                                         double pitch1,
                                         double dTdW1,
                                         double angle2,
                                         double pitch2,
                                         double dTdW2,
                                         double angle_target,
                                         double pitch_target);

    /// @} Wire geometry queries

    //......................................................................
    double Plane0Pitch(TPCID const& tpcid, unsigned int p1) const;
    double PlanePitch(TPCID const& tpcid, unsigned int p1 = 0, unsigned int p2 = 1) const;

    //--------------------------------------------------------------------------
    /// @{
    /// @name TPC channel mapping

    /// Returns the total number of channels present (not necessarily contiguous)
    virtual unsigned int Nchannels() const = 0;

    /// @brief Returns the number of channels in the specified ROP
    /// @return number of channels in the specified ROP, 0 if non-existent
    virtual unsigned int Nchannels(readout::ROPID const& ropid) const = 0;

    /// @brief Returns an std::vector<ChannelID_t> in all TPCs in a TPCSet
    std::vector<raw::ChannelID_t> ChannelsInTPCs() const;

    /// @brief Returns whether the specified channel is valid
    /// This default implementation assumes all channels up to Nchannels() valid.
    virtual bool HasChannel(raw::ChannelID_t channel) const
    {
      return raw::isValidChannelID(channel) ? channel < Nchannels() : false;
    }

    /// Returns a list of TPC wires connected to the specified readout channel ID
    /// @throws cet::exception (category: "Geometry") if non-existent channel
    virtual std::vector<WireID> ChannelToWire(raw::ChannelID_t channel) const = 0;

    /**
     * @brief Returns the view (wire orientation) on the specified TPC channel
     * @param channel TPC channel ID
     * @return the type of signal on the specified channel, or geo::kUnknown
     *
     * The view of the readout plane `channel` belongs to is returned, as in
     * `View(readout::ROPID const&) const`.
     */
    View_t View(raw::ChannelID_t const channel) const;

    /**
     * @brief Returns the view of the channels in the specified readout plane
     * @param ropid readout plane ID
     * @return the type of signal on the specified ROP
     *
     * Returns the view (wire orientation) on the channels of specified readout plane.  If
     * ropid is an invalid ID, geo::kUnknown is returned.  If ropid is a valid ID (i.e. an
     * ID whose isValid flag is set) that points to a non-existent readout plane, the
     * result is undefined.  Use HasROP() to check if the readout plane actually exists.
     */
    View_t View(readout::ROPID const& ropid) const;

    /**
     * @brief Returns the type of signal on the channels of specified TPC plane
     * @param plane TPC plane ID
     * @return the type of signal on the specified plane, or geo::kMysteryType
     *
     * Assumes that all the channels on the plane have the same signal type.
     *
     * @todo verify that kMysteryType is returned on invalid plane
     */
    SigType_t SignalType(PlaneID const& pid) const;

    /**
     * @brief Return the signal type of the specified channel
     * @param channel ID of the channel
     * @return signal type of the channel, or geo::kMysteryType if not known
     *
     * On any type of error (e.g., invalid or unknown channel ID),
     * geo::kMysteryType is returned.
     */
    SigType_t SignalType(raw::ChannelID_t const channel) const;

    /**
     * @brief Return the signal type on the specified readout plane
     * @param ropid ID of the readout plane
     * @return signal type on the plane, or geo::kMysteryType if not known
     *
     * If the readout plane ID is marked invalid, geo::kMysteryType is returned.  If the
     * readout plane is not marked invalid, but it does not match an existing readout
     * plane, the result is undefined.
     *
     * The default implementation uses readout plane to channel mapping.  Other
     * implementation may decide to do the opposite.
     */
    SigType_t SignalType(readout::ROPID const& ropid) const;

    /**
     * @brief Returns the ID of the channel nearest to the specified position
     * @param worldLoc 3D coordinates of the point (world reference frame)
     * @param planeid ID of the wire plane the channel must belong to
     * @return the ID of the channel, or `raw::InvalidChannelID` if invalid wire
     * @bug on invalid wire, a `geo::InvalidWireError` exception is thrown
     *
     */
    raw::ChannelID_t NearestChannel(Point_t const& worldLoc, PlaneID const& planeid) const;

    /**
     * @brief Returns an intersection point of two channels
     * @param c1 one channel ID
     * @param c2 the other channel ID
     * @return WireIDIntersection object if intersection point was found; otherwise std::nullopt
     *
     * @todo what happens for channels from different TPCs?
     * @todo what happens for channels with multiple intersection points?
     *
     * @deprecated This is clearly not APA-aware
     */
    std::optional<WireIDIntersection> ChannelsIntersect(raw::ChannelID_t c1,
                                                        raw::ChannelID_t c2) const;

    /// Returns a list of the plane IDs in the whole detector
    virtual std::set<PlaneID> const& PlaneIDs() const = 0;

    /**
     * @brief Returns the channel ID a wire is connected to
     * @param wireID ID of the wire
     * @return the ID of the channel
     * @see PlaneWireToChannel(geo::WireID const&)
     *
     * Behaviour on an invalid or not present wires is undefined.
     */
    virtual raw::ChannelID_t PlaneWireToChannel(WireID const& wireID) const = 0;

    /// @}

    //--------------------------------------------------------------------------
    /// @{
    /// @name Optical detector channel mapping

    /**
     * @brief Returns the number of optical channels contained in some detectors
     * @param NOpDets number of optical detectors
     * @return optical channels contained in NOpDets detectors
     *
     * This function returns how many channels can be expected to be present in a detector
     * with NOpDets optical detectors. This is an upper limit, as not all channels have
     * necessarily to be present.
     *
     * For example: if a detector has four channels per optical detector, the returned
     * value will be four times the argument NOpDets. If there is a single channel on each
     * optical detector, the return value will be the value NOpDets (this is in fact the
     * fallback implementation). If each optical detector can have anywhere between 2 and
     * 12 channels, the returned value is 12 times NOpDets, and it will be an
     * overestimation of the number of channels.
     */
    virtual unsigned int NOpChannels(unsigned int NOpDets) const;

    /// Number of electronics channels for all the optical detectors
    unsigned int NOpChannels() const;

    /// Largest optical channel number
    unsigned int MaxOpChannel() const;

    /// Is this a valid OpChannel number?
    bool IsValidOpChannel(int opChannel) const;

    /**
     * @brief Returns the number of optical channels contained in some detectors
     * @param NOpDets number of optical detectors
     * @return optical channels contained in NOpDets detectors
     *
     * This function returns the first optical channel ID larger than the last channel ID
     * in a detector with NOpDets optical detectors (with the same logic as
     * `NOpChannels()`).  For example, in a detector with 32 channels with contiguous IDs
     * starting at 0, this function would return 32. If the channels started with ID 1,
     * this function would instead return 33 and if there were a 16 channel gap, so that
     * valid channels are from 0 to 15 and from 32 to 47, this function would return 48.
     */
    virtual unsigned int MaxOpChannel(unsigned int NOpDets) const;

    /**
     * @brief Returns the number of channels in the specified optical detectors
     * @param opDet ID of the chosen optical detector
     * @return optical channels contained in optical detector with ID opDet
     *
     * This function returns how many channels are actually present in the optical
     * detector with the specified ID.
     *
     * For example: if a detector has four channels per optical detector, the returned
     * value will be four, regardless opDet, and . If there is a single channel on each
     * optical detector, the return value will be 1, again ignoring opDet (this is in fact
     * the fallback implementation). If each optical detector can have anywhere between 2
     * and 12 channels, the returned value will be 2, 12, etc., that is the exact number
     * of channels in opDet.
     *
     * Although implementations are encouraged to return 0 on invalid optical detectors,
     * the actual return value in that case is undefined.
     */
    virtual unsigned int NOpHardwareChannels(unsigned int opDet) const;

    /**
     * @brief Returns whether the ID identifies a valid optical detector channel
     * @param opChannel channel number
     * @param NOpDets number of optical detectors in the detector
     * @return whether opChannel would be a valid channel
     *
     * The specification of the number of optical channels reflects the logic described in
     * `NOpChannel()`.
     */
    virtual bool IsValidOpChannel(unsigned int opChannel, unsigned int NOpDets) const;

    /**
     * @brief Returns the channel ID of the specified hardware channel
     * @param detNum optical detector ID
     * @param hwchannel hardware channel within the specified optical detector
     * @return ID of the channel identified by detector and hardware channel IDs
     *
     * If the input IDs identify a non-existing channel, the result is undefined.
     */
    virtual unsigned int OpChannel(unsigned int detNum, unsigned int hwchannel = 0) const;

    /**
     * @brief Returns the optical detector the specified optical channel belongs
     * @param opChannel the optical detector channel being queried
     * @return the optical detector the specified optical channel belongs to
     *
     * If the specified optical channel is invalid, behaviour is undefined.
     */
    virtual unsigned int OpDetFromOpChannel(unsigned int opChannel) const;

    /**
     * @brief Returns the optical detector the specified optical channel belongs
     * @param opChannel the optical detector channel being queried
     * @return the optical detector geometry object the specified optical channel belongs to
     *
     * If the specified optical channel is invalid, behaviour is undefined.
     */
    OpDetGeo const& OpDetGeoFromOpChannel(unsigned int opChannel) const;

    /**
     * @brief Returns the hardware channel number of specified optical channel
     * @param opChannel the optical detector channel being queried
     * @return the optical detector the specified optical channel belongs to
     *
     * If the specified optical channel is invalid, behaviour is undefined.
     */
    virtual unsigned int HardwareChannelFromOpChannel(unsigned int opChannel) const;

    /// @}

    //--------------------------------------------------------------------------
    /// @{
    /// @name Mapping of position to wires

    /**
     * @brief Returns the index of the wire nearest to the specified position
     * @param YPos y coordinate on the wire plane
     * @param ZPos z coordinate on the wire plane
     * @param planeID ID of the plane
     * @return an index interpolation between the two nearest wires
     * @see NearestWireID()
     *
     * Respect to NearestWireID(), this method returns a real number, representing a
     * continuous coordinate in the wire axis, with the round values corresponding to the
     * actual wires.
     *
     * The plane is required to be valid and exist in the detector. Otherwise, the
     * behaviour is undefined.
     */
    virtual double WireCoordinate(double YPos, double ZPos, PlaneID const& planeID) const = 0;

    /**
     * @brief Returns the ID of the wire nearest to the specified position
     * @param worldPos position to be tested
     * @param planeID plane containing the wire
     * @return the ID of the wire closest to worldPos in the specified plane
     * @throw InvalidWireIDError the ID found is not present in the detector
     * @see WireCoordinate(double, double, geo::PlaneID const&)
     *
     * The plane is required to be valid and exist in the detector. Otherwise, the
     * behaviour is undefined.  An exception is thrown if the wire that would be the
     * closest is actually not present; but no check is performed whether the specified
     * position is outside the wire plane: wires are extrapolated to be infinitely long.
     * In other words, the result can be trusted only as long as the position is within
     * the specified wire plane.
     */
    virtual WireID NearestWireID(Point_t const& worldPos, PlaneID const& planeID) const = 0;

    /// @}

    //--------------------------------------------------------------------------
    /// @name TPC set mapping
    /// @{
    /**
     * @brief Returns the total number of TPC sets in the specified cryostat
     * @param cryoid cryostat ID
     * @return number of TPC sets in the cryostat, or 0 if no cryostat found
     */
    virtual unsigned int NTPCsets(readout::CryostatID const& cryoid) const = 0;

    /// Returns the largest number of TPC sets any cryostat in the detector has
    virtual unsigned int MaxTPCsets() const = 0;

    /// Returns whether we have the specified TPC set
    /// @return whether the TPC set is valid and exists
    virtual bool HasTPCset(readout::TPCsetID const& tpcsetid) const = 0;

    readout::TPCsetID FindTPCsetAtPosition(Point_t const& worldLoc) const;

    /// Returns the ID of the TPC set tpcid belongs to
    virtual readout::TPCsetID TPCtoTPCset(TPCID const& tpcid) const = 0;

    /**
     * @brief Returns a list of ID of TPCs belonging to the specified TPC set
     * @param tpcsetid ID of the TPC set to convert into TPC IDs
     * @return the list of TPCs, empty if TPC set is invalid
     *
     * Note that the check is performed on the validity of the TPC set ID, that does not
     * necessarily imply that the TPC set specified by the ID actually exists. Check the
     * existence of the TPC set first (HasTPCset()).  Behaviour on valid, non-existent TPC
     * set IDs is undefined.
     */
    virtual std::vector<TPCID> TPCsetToTPCs(readout::TPCsetID const& tpcsetid) const = 0;

    /// Returns the ID of the first TPC belonging to the specified TPC set
    virtual TPCID FirstTPCinTPCset(readout::TPCsetID const& tpcsetid) const = 0;

    /// @} TPC set mapping

    //--------------------------------------------------------------------------
    /// @name Readout plane mapping
    /// @{
    /**
     * @brief Returns the total number of ROP in the specified TPC set
     * @param tpcsetid TPC set ID
     * @return number of readout planes in the TPC set, or 0 if no TPC set found
     *
     * Note that this methods explicitly check the existence of the TPC set.
     */
    virtual unsigned int NROPs(readout::TPCsetID const& tpcsetid) const = 0;

    /// Returns the largest number of ROPs a TPC set in the detector has
    virtual unsigned int MaxROPs() const = 0;

    /// Returns whether we have the specified ROP
    /// @return whether the readout plane is valid and exists
    virtual bool HasROP(readout::ROPID const& ropid) const = 0;

    /// Returns the ID of the ROP planeid belongs to
    virtual readout::ROPID WirePlaneToROP(PlaneID const& planeid) const = 0;

    /// Returns a list of ID of planes belonging to the specified ROP
    virtual std::vector<PlaneID> ROPtoWirePlanes(readout::ROPID const& ropid) const = 0;

    /// Returns the ID of the first plane belonging to the specified ROP
    virtual PlaneID FirstWirePlaneInROP(readout::ROPID const& ropid) const = 0;

    /**
     * @brief Returns a list of ID of TPCs the specified ROP spans
     * @param ropid ID of the readout plane
     * @return the list of TPC IDs, empty if readout plane ID is invalid
     *
     * Note that this check is performed on the validity of the readout plane ID, that
     * does not necessarily imply that the readout plane specified by the ID actually
     * exists. Check if the ROP exists with HasROP().  The behaviour on non-existing
     * readout planes is undefined.
     */
    virtual std::vector<TPCID> ROPtoTPCs(readout::ROPID const& ropid) const = 0;

    /**
     * @brief Returns the ID of the ROP the channel belongs to
     * @return the ID of the ROP the channel belongs to (invalid if channel is)
     * @see HasChannel()
     *
     * The channel must exist, or be the invalid channel value.  With a channel that is
     * not present in the mapping and that is not the invalid channel
     * (`raw::InvalidChannelID`), the result is undefined.
     */
    virtual readout::ROPID ChannelToROP(raw::ChannelID_t channel) const = 0;

    /**
     * @brief Returns the ID of the first channel in the specified readout plane
     * @param ropid ID of the readout plane
     * @return ID of first channel, or raw::InvalidChannelID if ID is invalid
     *
     * Note that this check is performed on the validity of the readout plane ID, that
     * does not necessarily imply that the readout plane specified by the ID actually
     * exists. Check if the ROP exists with HasROP().  The behaviour for non-existing
     * readout planes is undefined.
     */
    virtual raw::ChannelID_t FirstChannelInROP(readout::ROPID const& ropid) const = 0;

    /// @}

    //--------------------------------------------------------------------------
    /// @{
    /// @name Testing (not in the interface)

    /// Retrieve the private fFirstChannelInNextPlane vector for testing.
    std::vector<std::vector<std::vector<raw::ChannelID_t>>> const& FirstChannelInNextPlane() const
    {
      return fFirstChannelInThisPlane;
    }

    /// Retrieve the private fFirstChannelInThisPlane vector for testing.
    std::vector<std::vector<std::vector<raw::ChannelID_t>>> const& FirstChannelInThisPlane() const
    {
      return fFirstChannelInNextPlane;
    }

    /// @}

    //--------------------------------------------------------------------------

  protected:
    /**
     * @brief Return the signal type of the specified channel
     * @param channel ID of the channel
     * @return signal type of the channel, or geo::kMysteryType if not known
     *
     * On any type of error (e.g., invalid or unknown channel ID), geo::kMysteryType is
     * returned.
     */
    virtual SigType_t SignalTypeForChannelImpl(raw::ChannelID_t const channel) const = 0;

    /**
     * @brief Return the signal type on the specified readout plane
     * @param ropid ID of the readout plane
     * @return signal type on the plane, or geo::kMysteryType if not known
     *
     * If the readout plane ID is marked invalid, geo::kMysteryType is returned.  If the
     * readout plane is not marked invalid, but it does not match an existing readout
     * plane, the result is undefined.
     *
     * The default implementation uses readout plane to channel mapping.
     * Other implementation may decide to do the opposite.
     */
    virtual SigType_t SignalTypeForROPIDImpl(readout::ROPID const& ropid) const;

    /// Data type for per-TPC information
    template <typename T>
    using TPCInfoMap_t = std::vector<std::vector<T>>;

    /// Data type for per-plane information
    template <typename T>
    using PlaneInfoMap_t = TPCInfoMap_t<std::vector<T>>;

    /**
    * @name Internal structure data access
    *
    * These functions allow access to the XxxInfoMap_t types based on geometry element
    * IDs.  They are strictly internal.
    */
    /// @{

    /// Returns the specified element of the TPC map
    template <typename T>
    T const& AccessElement(TPCInfoMap_t<T> const& map, TPCID const& id) const
    {
      return map[id.Cryostat][id.TPC];
    }

    /// Returns the number of elements in the specified cryostat of the TPC map
    template <typename T>
    size_t AccessElementSize(TPCInfoMap_t<T> const& map, CryostatID const& id) const
    {
      return map[id.Cryostat].size();
    }

    //@{
    /// Returns whether the ID specifies a valid entry
    template <typename T>
    bool isValidElement(TPCInfoMap_t<T> const& map, CryostatID const& id) const
    {
      return id.Cryostat < map.size();
    }
    template <typename T>
    bool isValidElement(TPCInfoMap_t<T> const& map, TPCID const& id) const
    {
      return isValidElement(map, id.asCryostatID()) && (id.TPC < map[id.Cryostat].size());
    }
    //@}

    /// Returns the specified element of the plane map
    template <typename T>
    T const& AccessElement(PlaneInfoMap_t<T> const& map, PlaneID const& id) const
    {
      return map[id.Cryostat][id.TPC][id.Plane];
    }

    /// Returns the number of elements in the specified TPC of the plane map
    template <typename T>
    size_t AccessElementSize(PlaneInfoMap_t<T> const& map, TPCID const& id) const
    {
      return map[id.Cryostat][id.TPC].size();
    }

    //@{
    /// Returns whether the ID specifies a valid entry
    template <typename T>
    bool isValidElement(PlaneInfoMap_t<T> const& map, CryostatID const& id) const
    {
      return id.Cryostat < map.size();
    }
    template <typename T>
    bool isValidElement(PlaneInfoMap_t<T> const& map, TPCID const& id) const
    {
      return isValidElement(map, id.asCryostatID()) && (id.TPC < map[id.Cryostat].size());
    }
    template <typename T>
    bool isValidElement(PlaneInfoMap_t<T> const& map, PlaneID const& id) const
    {
      return isValidElement(map, id.asTPCID()) && (id.Plane < AccessSize(map, id.asTPCID()));
    }
    //@}

    /// Returns a pointer to the specified element, or nullptr if invalid
    template <typename T>
    T const* GetElementPtr(PlaneInfoMap_t<T> const& map, PlaneID const& id) const
    {
      if (id.Cryostat >= map.size()) return nullptr;
      auto const& cryo_map = map[id.Cryostat];
      if (id.TPC >= cryo_map.size()) return nullptr;
      auto const& TPC_map = cryo_map[id.TPC];
      if (id.Plane >= TPC_map.size()) return nullptr;
      auto const& plane_map = TPC_map[id.Plane];
      return &plane_map;
    } // GetElementPtr()

    ///@} Internal structure data access

    // These 3D vectors are used in initializing the Channel map.  Only a 1D vector is
    // really needed so far, but these are more general.
    PlaneInfoMap_t<raw::ChannelID_t> fFirstChannelInThisPlane;
    PlaneInfoMap_t<raw::ChannelID_t> fFirstChannelInNextPlane;

  private:
    /// Wire ID check for WireIDsIntersect methods
    bool WireIDIntersectionCheck(WireID const& wid1, WireID const& wid2) const;

    /// Recomputes the drift direction; needs planes to have been initialised.
    void ResetDriftDirection(TPCID const& tpcid);

    void UpdateAfterSorting(TPCGeo const& tpc, std::vector<PlaneGeo>& planes);

    /// Sorts (in place) the specified `PlaneGeo` objects by drift distance.
    void SortPlanes(TPCID const& tpcid, std::vector<PlaneGeo>& planes) const;
    void SortSubVolumes(std::vector<PlaneGeo>& planes, Compare<WireGeo> compareWires) const;

    std::map<TPCID, std::vector<PlaneGeo>> fPlanes;
    std::map<TPCID, std::vector<double>> fPlane0Pitch; ///< Pitch between planes.

    // cached values
    std::set<View_t> allViews; ///< All views in the detector.

    GeometryCore const* fGeom;
  };

}
#endif // LARCOREALG_GEOMETRY_WIREREADOUTGEOM_H
