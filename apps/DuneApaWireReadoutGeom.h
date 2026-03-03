////////////////////////////////////////////////////////////////////////
/// \file  DuneApaWireReadoutGeom.h
/// \brief Interface to algorithm class for a specific detector channel mapping
///
/// \version $Id:  $
/// \author  David Adams
///
/// David Adams
/// November 2016
///
/// Derived from WireReadoutAPAGeom by Tyler Alion.
///
/// Maps wires to channels for a DUNE-like APA structure. Each each APA has two
/// wrapped induction (u and v) ROPs (readout planes) shared by two TPCs and
/// separate collection (z) planes for each of the TPCs. I.e. the APA has four ROPs.
///
/// The number of channels for the induction view is determined by the number of
/// wires which reach the starting side of each of the TPCs. These are called
/// anchored wires. For collection planes, the numbers or wires and anchored
/// wires are the same.
///
/// Geometry is indexed by cryostat, TPC, plane and wire: (cry, tpc, pla, wir).
/// Readout is indexed by cryostat, APA, ROP and ROP plane: (cry, apa, rop, rpl).
/// All these indices are relative.
/// Channel numbers (cha) are absolute with ordering following that of the readout.
///
/// Optical detector flag determines how optical channel mapping is done:
//    OpDetFlag = 0 - Simple mapping with ChannelsPerOpDet fore each optical detector
//    OpDetFlag = 1 - Dune 35t mapping
////////////////////////////////////////////////////////////////////////
#ifndef geo_DuneApaWireReadoutGeom_H
#define geo_DuneApaWireReadoutGeom_H

#include <vector>
#include <set>
//#include "cetlib_except/exception.h"
#include "larcoreobj/SimpleTypesAndConstants/readout_types.h" // readout::ROPID, ...
#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h" // raw::ChannelID_t
#include "larcorealg/Geometry/WireReadoutGeom.h"
#include "larcorealg/Geometry/WireReadoutSorter.h"
#include "fhiclcpp/fwd.h"

namespace geo{

class DuneApaWireReadoutGeom : public WireReadoutGeom {

public:

  DuneApaWireReadoutGeom(fhicl::ParameterSet const& pset,
                         GeometryCore const* geom,
                         std::unique_ptr<WireReadoutSorter> sorter);
    
  /// Returns a list of TPC wires connected to the specified readout channel ID
  /// @throws cet::exception (category: "Geometry") if non-existent channel
  std::vector<WireID> ChannelToWire(raw::ChannelID_t channel) const override;
    
  unsigned int Nchannels() const override;
    
  /// @brief Returns the number of channels in the specified ROP
  /// @return number of channels in the specified ROP, 0 if non-existent
  /// @todo to be completed
  unsigned int Nchannels(readout::ROPID const& ropid) const override;
    
  double WireCoordinate(double YPos,
                        double ZPos,
                        geo::PlaneID const& planeID) const override;
    
  //@{
  virtual WireID
  NearestWireID(const geo::Point_t& worldPos, geo::PlaneID const& planeID) const override;
  //@}
  //@{
  virtual raw::ChannelID_t PlaneWireToChannel(geo::WireID const& wireID) const override;
  //@}
  SigType_t                SignalTypeForChannelImpl( raw::ChannelID_t const channel) const override;
  std::set<PlaneID> const& PlaneIDs()                                  const override;

  //
  // TPC set interface
  //
  /// @name TPC set mapping
  /// @{
  /**
   * @brief Returns the total number of TPC sets in the specified cryostat
   * @param cryoid cryostat ID
   * @return number of TPC sets in the cryostat, or 0 if no cryostat found
   */
  virtual unsigned int NTPCsets(readout::CryostatID const& cryoid) const override;
    
  /// Returns the largest number of TPC sets any cryostat in the detector has
  /// @todo to be completed
  virtual unsigned int MaxTPCsets() const override;
    
  /// Returns whether we have the specified TPC set
  /// @return whether the TPC set is valid and exists
  /// @todo to be completed
  virtual bool HasTPCset(readout::TPCsetID const& tpcsetid) const override;
    
  /**
   * @brief Returns the ID of the TPC set the specified TPC belongs to
   * @param tpcid ID of the TPC
   * @return the ID of the corresponding TPC set
   */
  virtual readout::TPCsetID TPCtoTPCset(geo::TPCID const& tpcid) const override;
    
  /**
   * @brief Returns a list of ID of TPCs belonging to the specified TPC set
   * @param tpcsetid ID of the TPC set to convert into TPC IDs
   * @return the list of TPCs, empty if TPC set is invalid
   */
  virtual std::vector<geo::TPCID> TPCsetToTPCs(readout::TPCsetID const& tpcsetid) const override;
    
  /// Returns the ID of the first TPC belonging to the specified TPC set
  /// @todo to be completed
  virtual geo::TPCID FirstTPCinTPCset(readout::TPCsetID const& tpcsetid) const override;
    
  /// @} TPC set mapping
    
  //
  // Readout plane interface
  //
  /// @name Readout plane mapping
  /// @{

  /**
   * @brief Returns the total number of ROP in the specified TPC set
   * @param tpcsetid TPC set ID
   * @return number of readout planes in the TPC set, or 0 if no TPC set found
   * 
   * Note that this methods explicitly check the existence of the TPC set.
   */
  virtual unsigned int NROPs(readout::TPCsetID const& tpcsetid) const override;
    
  /// Returns the largest number of ROPs a TPC set in the detector has
  /// @todo to be completed
  virtual unsigned int MaxROPs() const override;
    
  /// Returns whether we have the specified ROP
  /// @return whether the readout plane is valid and exists
  virtual bool HasROP(readout::ROPID const& ropid) const override;
    
  /**
   * @brief Returns the ID of the ROP planeid belongs to, or invalid if none
   * @param planeid ID of the wire plane
   * @return the ID of the corresponding ROP, or invalid ID when planeid is
   *
   * Note that this check is performed on the validity of the wire plane
   * ID, that does not necessarily imply that the wire plane specified by
   * the ID actually exists. Check if the wire plane exists with HasPlane().
   * The behaviour on non-existing wire planes is undefined.
   */
  virtual readout::ROPID WirePlaneToROP(geo::PlaneID const& planeid) const override;
    
  /**
   * @brief Returns a list of ID of wire planes belonging to the specified ROP
   * @param ropid ID of the readout plane to convert into wire planes
   * @return the list of wire plane IDs, empty if readout plane ID is invalid
   *
   * Note that this check is performed on the validity of the readout plane
   * ID, that does not necessarily imply that the readout plane specified by
   * the ID actually exists. Check if the ROP exists with HasROP().
   * The behaviour on non-existing readout planes is undefined.
   */
  virtual std::vector<geo::PlaneID> ROPtoWirePlanes(readout::ROPID const& ropid) const override;
    
  /**
   * @brief Returns a list of ID of TPCs the specified ROP spans
   * @param ropid ID of the readout plane
   * @return the list of TPC IDs, empty if readout plane ID is invalid
   *
   * Note that this check is performed on the validity of the readout plane
   * ID, that does not necessarily imply that the readout plane specified by
   * the ID actually exists. Check if the ROP exists with HasROP().
   * The behaviour on non-existing readout planes is undefined.
   */
  virtual std::vector<geo::TPCID> ROPtoTPCs(readout::ROPID const& ropid) const override;
    
  /// Returns the ID of the ROP the channel belongs to (invalid if none)
  /// @throws cet::exception (category: "Geometry") if non-existent channel
  virtual readout::ROPID ChannelToROP(raw::ChannelID_t channel) const override;
    
  /**
   * @brief Returns the ID of the first channel in the specified readout plane
   * @param ropid ID of the readout plane
   * @return ID of first channel, or raw::InvalidChannelID if ID is invalid
   * 
   * Note that this check is performed on the validity of the readout plane
   * ID, that does not necessarily imply that the readout plane specified by
   * the ID actually exists. Check if the ROP exists with HasROP().
   * The behaviour for non-existing readout planes is undefined.
   */
  virtual raw::ChannelID_t FirstChannelInROP(readout::ROPID const& ropid) const override;
    
  /**
   * @brief Returns the ID of the first plane belonging to the specified ROP
   * @param ropid ID of the readout plane
   * @return ID of first channel, or raw::InvalidChannelID if ID is invalid
   * 
   * Note that this check is performed on the validity of the readout plane
   * ID, that does not necessarily imply that the readout plane specified by
   * the ID actually exists. Check if the ROP exists with HasROP().
   * The behaviour for non-existing readout planes is undefined.
   */
  virtual geo::PlaneID FirstWirePlaneInROP(readout::ROPID const& ropid) const override;
    
  /// @} readout plane mapping
    
  unsigned int NOpChannels(unsigned int NOpDets)                        const override;
  unsigned int NOpHardwareChannels(unsigned int opDet)                  const override;
  unsigned int OpChannel(unsigned int detNum, unsigned int channel = 0) const override;
  unsigned int OpDetFromOpChannel(unsigned int opChannel)               const override;
  unsigned int HardwareChannelFromOpChannel(unsigned int opChannel)     const override;
    
protected:

  template<class T>
  using Vector = std::vector<T>;
  template<class T>
  using TwoVector = Vector<Vector<T>>;
  template<class T>
  using ThreeVector = Vector<TwoVector<T>>;
  template<class T>
  using FourVector = Vector<ThreeVector<T>>;
  unsigned int                         fOpDetFlag;             /// Flag for OpDet channel map.
  unsigned int                         fChannelsPerOpDet;
  unsigned int                         fNcryostat;             ///< number of cryostats in the detector
  unsigned int                         fNchannels;             ///< number of channels in the detector
  unsigned int                         fChannelsPerAPA;        ///< number of channels in each APA
  raw::ChannelID_t                     fTopChannel;            ///< book keeping highest channel #
  unsigned int                         fNTpcMax;               ///< Max # TPCs in any cryostat
  unsigned int                         fNApaMax;               ///< Max # TPCs in any cryostat
  unsigned int                         fNRopMax;               ///< Max # ROPs in any APA
  std::vector<unsigned int>            fNTpc;                  ///< number of TPCs in each cryostat
  std::vector<unsigned int>            fNApa;                  ///< number of APAs in each cryostat
  std::set<View_t>                     fViews;                 ///< vector of the views present in the detector
  std::set<PlaneID>                    fPlaneIDs;              ///< vector of the PlaneIDs present in the detector
  ThreeVector<unsigned int>            fWiresPerPlane;         ///< # wires/TPC plane for each (cry, tpc, pla)
  TwoVector<unsigned int>              fPlanesPerTpc;          ///< # planes for each (cry, tpc)
  TwoVector<unsigned int>              fRopsPerApa;            ///< # ROPs for each (cry, apa)
  ThreeVector<unsigned int>            fApaTpcs;               ///< TPCs for each APA
  ThreeVector<unsigned int>            fPlanesPerRop;          ///< # TPC planes for each (cry, apa, rop)
  FourVector<unsigned int>             fRopTpc;                ///< # TPC planes for each (cry, apa, rop, rpl)
  FourVector<unsigned int>             fRopPlane;              ///< # TPC plane index for each (cry, apa, rop, rpl)
  TwoVector<unsigned int>              fChannelsPerApa;        ///< # channels for each APA
  ThreeVector<unsigned int>            fChannelsPerRop;        ///< # channels for each Rop
  ThreeVector<unsigned int>            fAnchoredWires;         ///< # anchored wires for each (cry, tpc, pla)
  ThreeVector<unsigned int>            fPlaneApa;              ///< APA for each TPC plane (cry, tpc, pla)
  ThreeVector<unsigned int>            fPlaneRop;              ///< ROP for each TPC plane (cry, tpc, pla)
  ThreeVector<unsigned int>            fPlaneRopIndex;         ///< Index in ROP for each TPC plane (cry, tpc, pla)
  ThreeVector<unsigned int>            fAnchoredPlaneRop;      ///< ROP holding the anchored wires for (cry, tpc, pla)
  ThreeVector<unsigned int>            fWiresPerRop;           ///< # wires/ROP for each (cry, apa, rop)
  PlaneInfoMap_t<raw::ChannelID_t>     fFirstChannelInThisRop; ///<  (cry, apa, rop)
  PlaneInfoMap_t<raw::ChannelID_t>     fFirstChannelInNextRop; ///<  (cry, apa, rop)

  /// all data we need for each APA
  typedef struct {
    double fFirstWireCenterY;
    double fFirstWireCenterZ;
    /// +1 if the wire ID order follow z (larger z, or smaller intercept => larger wire ID); -1 otherwise
    float fWireSortingInZ;
    double fYmax;
    double fYmin;
    double fZmax;
    double fZmin;
  } PlaneData_t;
    
  ///< collects all data we need for each plane (indices: c t p)
  PlaneInfoMap_t<PlaneData_t>                          fPlaneData;
    
  std::vector< double > fWirePitch;
  std::vector< double > fOrientation;
  std::vector< double > fSinOrientation; // to explore improving speed
    std::vector< double > fCosOrientation; // to explore improving speed

    /// Returns whether the specified ID represents a valid cryostat.
    bool HasCryostat(CryostatID const& cid) const
      { return cid.Cryostat < fNcryostat; }
    
  }; // class DuneApaWireReadoutGeom
  
  //----------------------------------------------------------------------------
  
  
}
#endif
