/**
 * @file channel-to-wire.cxx
 *
 * Developer(s) of this DAQ application have yet to replace this line with a brief description of the application.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include <stdio.h>		// printf
#include <vector>
#include <map>
//#include "DuneApaWireReadoutGeom.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h" // geo::WireID
#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h"  // raw::ChannelID_t

#include "TRACE/trace.h"

#if 0
namespace geo {
  class WireGeo;
  class PlaneGeo {
  public:
    using WireCollection_t = std::vector<WireGeo>;
    /// Number of wires in this plane
    unsigned int Nwires() const { return fWire.size(); }
    unsigned int NElements() const { return Nwires(); }
  private:
    WireCollection_t fWire;           ///< List of wires in this plane.
  };
}
#endif

using std::vector;
using raw::ChannelID_t;
using geo::PlaneID;
using geo::WireID;
#if 0
using geo::PlaneGeo;
#endif

typedef unsigned int Index;
Index badIndex = 999999;


template <typename T> using TPCInfoMap_t = std::vector<std::vector<T>>;
template <typename T> using PlaneInfoMap_t = TPCInfoMap_t<std::vector<T>>;
template <typename T> using Vector = std::vector<T>;
template <typename T> using TwoVector = Vector<Vector<T>>;
template <typename T> using ThreeVector = Vector<TwoVector<T>>;
template <typename T> using FourVector = Vector<ThreeVector<T>>;


namespace geo {
class DuneApaWireReadoutGeom {
public:
  DuneApaWireReadoutGeom();
  std::vector<WireID> ChannelToWire(ChannelID_t icha) const;

#if 0
  inline PlaneGeo const* PlanePtr(PlaneID const& planeid) const
  {
    auto const [tpc_id, plane] = std::make_pair(planeid.parentID(), planeid.Plane);
    auto it = fPlanes.find(tpc_id);
    if (it == fPlanes.cend()) { return nullptr; }
    if (std::size_t const n = it->second.size(); n <= plane) { return nullptr; }
    return &it->second[plane];
  }

  inline unsigned int Nwires(PlaneID const& planeid) const
  {
    PlaneGeo const* pPlane = PlanePtr(planeid);
    return pPlane ? pPlane->NElements() : 0;
  }
#endif
  
protected:
#if 0
  std::map<TPCID, std::vector<PlaneGeo>> fPlanes;
#endif
  unsigned int                      fNcryostat;             ///< number of cryostats in the detector
  unsigned int                      fNchannels;             ///< number of channels in the detector

  std::vector<unsigned int>         fNApa;                  ///< number of APAs in each cryostat

  PlaneInfoMap_t<raw::ChannelID_t>  fFirstChannelInThisRop; ///<  (cry, apa, rop)
  PlaneInfoMap_t<raw::ChannelID_t>  fFirstChannelInNextRop; ///<  (cry, apa, rop)

  TwoVector<unsigned int>           fRopsPerApa;            ///< # ROPs for each (cry, apa)
  ThreeVector<unsigned int>         fPlanesPerRop;          ///< # TPC planes for each (cry, apa, rop)
  ThreeVector<unsigned int>         fWiresPerPlane;         ///< # wires/TPC plane for each (cry, tpc, pla)
  ThreeVector<unsigned int>         fAnchoredWires;         ///< # anchored wires for each (cry, tpc, pla)
  FourVector<unsigned int>          fRopTpc;                ///< # TPC planes for each (cry, apa, rop, rpl)
  FourVector<unsigned int>          fRopPlane;              ///< # TPC plane index for each (cry, apa, rop, rpl)

};
}

using geo::DuneApaWireReadoutGeom;

DuneApaWireReadoutGeom::
DuneApaWireReadoutGeom()
{
  Index ncry = 1;
  fNchannels = 1280;
  fNcryostat = ncry;                   TLOG()<<"Initialize fNcryostat = "<<ncry;
  fNApa.resize(ncry);                  TLOG()<<"Initialize fNApa.resize("<<ncry<<")";
  fWiresPerPlane.resize(ncry);
  fAnchoredWires.resize(ncry);         TLOG()<<"Initialize fAnchoredWires.resize("<<ncry<<")";
  fFirstChannelInThisRop.resize(ncry); TLOG()<<"Initialize fFirstChannelInThisRop.resize("<<ncry<<")";
  fFirstChannelInNextRop.resize(ncry); TLOG()<<"Initialize fFirstChannelInNextRop.resize("<<ncry<<")";
  fRopsPerApa.resize(ncry);            TLOG()<<"Initialize fRopsPerApa.resize("<<ncry<<")";
  fPlanesPerRop.resize(ncry);          TLOG()<<"Initialize fPlanesPerRop.resize("<<ncry<<")";
  fRopTpc.resize(ncry);                TLOG()<<"Initialize fRopTpc.resize("<<ncry<<")";
  fRopPlane.resize(ncry);              TLOG()<<"Initialize fRopPlane.resize("<<ncry<<")";
  for ( Index icry=0; icry<ncry; ++icry) {
    Index ntpc = 2;
    Index napa = ntpc/2;  // Assume 1 APA for every two TPCs
    fNApa[icry] = napa;                        TLOG()<<"Initialize fNApa["<<icry<<"] = "<<napa;
    fWiresPerPlane[icry].resize(ntpc);
    fAnchoredWires[icry].resize(ntpc);         TLOG()<<"Initialize fAnchoredWires["<<icry<<"].resize("<<ntpc<<")";
    fRopsPerApa[icry].resize(napa, 4);         TLOG()<<"Initialize fRopsPerApa["<<icry<<"].resize("<<napa<<",4)";
    fPlanesPerRop[icry].resize(napa);          TLOG()<<"Initialize fPlanesPerRop["<<icry<<"].resize("<<napa<<")";
    fFirstChannelInThisRop[icry].resize(napa); TLOG()<<"Initialize fFirstChannelInThisRop["<<icry<<"].resize("<<napa<<")";
    fFirstChannelInNextRop[icry].resize(napa); TLOG()<<"Initialize fFirstChannelInNextRop["<<icry<<"].resize("<<napa<<")";
    fRopTpc[icry].resize(napa);                TLOG()<<"Initialize fRopTpc["<<icry<<"].resize("<<napa<<")";
    fRopPlane[icry].resize(napa);              TLOG()<<"Initialize fRopPlane["<<icry<<"].resize("<<napa<<")";
    for ( Index itpc=0; itpc<ntpc; ++itpc ) {
      Index npla = 3; /* U, V, Z */            TLOG()<<"npla="<<npla;
      fAnchoredWires[icry][itpc].resize(npla, 0);
      fWiresPerPlane[icry][itpc].resize(npla, 0);
      for ( Index ipla=0; ipla<npla; ++ipla ) {
        Index nwir = 0;//Nwires({tpcid, ipla});
	if      (itpc==0 && ipla==0) nwir = 316;
	else if (itpc==0 && ipla==1) nwir = 315;
	else if (itpc==0 && ipla==2) nwir = 240;
	else if (itpc==1 && ipla==0) nwir = 316;
	else if (itpc==1 && ipla==1) nwir = 315;
	else if (itpc==1 && ipla==2) nwir = 240;
	else {
	  TLOG()<<"icry="<<icry<<" itpc="<<itpc<<" ipla="<<ipla<<" nwir="<<nwir;
	  TLOG_ERROR() << "unexpected cryo, tpc, and/or plane";
	  exit(1);
	}
        fWiresPerPlane[icry][itpc][ipla] = nwir;
	TLOG()<<"fWiresPerPlane[icry="<<icry<<"][itpc="<<itpc<<"][ipla="<<ipla<<"]="<<nwir;
      }
    }
    Index itpc = 0;
    for ( Index iapa=0; iapa<napa; ++iapa ) {
      Index nrop = fRopsPerApa[icry][iapa];
      fFirstChannelInThisRop[icry][iapa].resize(nrop, 0);
      fFirstChannelInNextRop[icry][iapa].resize(nrop, 0);
      fPlanesPerRop[icry][iapa].resize(nrop, 0);
      fRopTpc[icry][iapa].resize(nrop);
      fRopPlane[icry][iapa].resize(nrop);
      Index ipla = 0;
      // Induction planes
      for ( Index irop=0; irop<2; ++irop ) {
        fPlanesPerRop[icry][iapa][irop] = 2;
        fRopTpc[icry][iapa][irop].push_back(itpc);
        fRopTpc[icry][iapa][irop].push_back(itpc+1);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        ++ipla;
      }
      // Collection planes.
      for ( Index irop=2; irop<4; ++irop ) {
        fPlanesPerRop[icry][iapa][irop] = 1;
        fRopTpc[icry][iapa][irop].push_back(itpc);
        fRopPlane[icry][iapa][irop].push_back(ipla);
        ++itpc;
      }
    }
  }

  // Map planes to ROPs and wires to channels.
  // We use the geometry to deduce the numbers of channels/plane.
  // For induction planes, we look for the first adjacent pair of wires
  // with the same center z-position and assume the index of the first
  // of those gives the count of wires read out from one side.
  // These are called anchored wires.
  ChannelID_t icha = 0;
  for ( Index icry=0; icry!=ncry; ++icry ) {
    Index napa = fNApa[icry];
    for ( Index iapa=0; iapa!=napa; ++iapa ) {
      Index nrop = fRopsPerApa[icry][iapa];
      for ( Index irop=0; irop!=nrop; ++irop ) {
        Index nrpl = fPlanesPerRop[icry][iapa][irop];
        fFirstChannelInThisRop[icry][iapa][irop] = icha;
        for ( Index irpl=0; irpl!=nrpl; ++irpl ) {
	  Index itpc = fRopTpc[icry][iapa][irop][irpl];            TLOG()<<"itpc="<<itpc;
	  Index ipla = fRopPlane[icry][iapa][irop][irpl];
	  const Vector<View_t> eview = {geo::kU, geo::kV, geo::kZ};
	  View_t view=eview[ipla];
	  TLOG_DEBUG(3)<<"icry="<<icry<<" iapa="<<iapa<<" irop="<<irop<<" irpl="<<irpl<<" view="<<view<<" eview[ipla]="<<eview[ipla];
	  //...
	  Index nAnchoredWires = 0;  // # wires from this TPC plane contributing to the ROP
	  Index nwir = fWiresPerPlane[icry][itpc][ipla];
	  if ( view == geo::kZ ) {
	    nAnchoredWires = nwir;
	    TLOG()<<"InitializeA nAnchoredWires="<<nAnchoredWires;
          // Induction planes.
          } else {
# if 0
            for ( unsigned int iwir=0; iwir+1<nwir; ++iwir ) {
              auto const xyz = plageo.Wire(iwir).GetCenter();
              auto const xyz_next = plageo.Wire(iwir+1).GetCenter();
              if ( xyz.Z() == xyz_next.Z() ) {
                nAnchoredWires = iwir;
		TLOG()<<"InitializeB nAnchoredWires="<<nAnchoredWires;
                break;
              }
            }
# endif
	    TLOG_DEBUG(6)<<"InitializeC nAnchoredWires="<<nAnchoredWires;
          }
	  // Tom Junk: a hack for iceberg geometry -- the assumption that Z doesnt change for a wire center in common wires
	  // in the code above calculating nAnchoredWires doesn't work for iceberg and nAnchoredWires ends up being zero.
	  // Put some conditions in here so it is unlikely to be triggered in non-Iceberg cases
	  // if nAnchordWires == 0 subsequent code will crash on an integer divide by zero

	  TLOG_DEBUG() << "nAnchoredWires="<<nAnchoredWires<<" nwir="<<nwir<<" view="<<view;
	  if (nAnchoredWires == 0 && nwir >310 && nwir < 320 && view != geo::kZ) {
	    nAnchoredWires = 200;
	    TLOG_DEBUG(5)<<"setting nAnchoredWires = 200";
	  }
	  
          fAnchoredWires[icry][itpc][ipla] = nAnchoredWires;TLOG()<<"Initialize fAnchoredWires["<<icry<<"]["<<itpc<<"]["<<ipla<<"] = "<<nAnchoredWires;

          icha += nAnchoredWires;
        }
        fFirstChannelInNextRop[icry][iapa][irop] = icha;
      }
    }
  }

  for ( Index icry=0; icry<ncry; ++icry ) {
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "Cryostat " << icry << ":"; 
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "  " << fNchannels << " total channels"; 
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "  For all identical APA:" ; 
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "    U channels per APA = " << 2*fAnchoredWires[0][0][0] ;
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "    V channels per APA = " << 2*fAnchoredWires[0][0][1] ;
    TLOG_DEBUG("DuneApaWireReadoutGeom") << "    Z channels per APA = " << 2*fAnchoredWires[0][0][2] ;
  }

}

std::vector<WireID> DuneApaWireReadoutGeom::ChannelToWire(ChannelID_t icha) const {
  vector< WireID > wirids;
  if ( icha >= fNchannels ) return wirids;
  // Loop over ROPs to find the one holding this channel.
  Index ncry = fNcryostat;
  Index icry = badIndex;
  Index iapa = badIndex;
  Index irop = badIndex;
  Index ichaRop = badIndex;    // Channel number in the ROP
  bool found = false;
  for ( icry=0; icry<ncry; ++icry ) {
    Index napa = fNApa[icry];
    TLOG_DEBUG(0) << "icha="<<icha<<" napa="<<napa<<" fNApa.size()="<<fNApa.size();
    for ( iapa=0; iapa<napa; ++iapa ) {
      Index nrop = fRopsPerApa[icry][iapa];
      for ( irop=0; irop<nrop; ++irop ) {
        Index icha1 = fFirstChannelInThisRop[icry][iapa][irop];
        Index icha2 = fFirstChannelInNextRop[icry][iapa][irop];
        found = icha >= icha1 && icha < icha2;
	TLOG_DEBUG(1) << "found="<<found<<" icry="<<icry<<" iapa="<<iapa<<" irop="<<irop<<" icha1="<<icha1<<" icha2="<<icha2;
        if ( found ) {
          ichaRop = icha - icha1;
	  TLOG_DEBUG(2)<<"found! ichaRop="<<ichaRop;
          break;
        }
      }
      if ( found ) break;
    }
    if ( found ) break;
  }
  if ( icry >= ncry ) {
    TLOG_ERROR("DuneApaWireReadoutGeom") << "Unable to find APA plane for channel " << icha;
    exit (1);
  }
  // Extract TPC(s) from ROP
  Index nrpl = fPlanesPerRop[icry][iapa][irop];
  if ( nrpl == 0 ) { TLOG_ERROR() << __func__ << ": No TPC planes."; exit (1); }
  if ( nrpl > 2 ) { TLOG_ERROR() << __func__ << ": Too many TPC planes."; exit (1); }
  Index itpc1 = fRopTpc[icry][iapa][irop][0];
  Index ipla = fRopPlane[icry][iapa][irop][0];
  Index itpc2 = (nrpl > 1 ) ? fRopTpc[icry][iapa][irop][1] : itpc1;
  Index nAnchored = fAnchoredWires[icry][itpc1][ipla];
  bool wrapped = ipla < 2;
  TLOG_DEBUG(3) << "wrapped="<<wrapped;
  if ( wrapped && itpc2 == itpc1 )
    { TLOG_ERROR() << __func__ << ": 2nd plane not found for wrapped ROP"; exit (1); }
  if ( wrapped && ipla != fRopPlane[icry][iapa][irop][1] )
    { TLOG_ERROR() << __func__ << ": Wrapped planes have inconsistent indices."; exit (1); }
  // For now, assume the second TPC plane has the same # anchored wires.
  // Code will need some work if we want to relax this assumption.
  if (  wrapped && fAnchoredWires[icry][itpc2][ipla] != nAnchored )
    { TLOG_ERROR() << __func__ << ": Planes have inconsistent anchor counts."; exit (1); }
  // If this is a wrapped ROP and the wire number is larger than nAnchored, then
  // the first wire for this channel is in the other TPC plane.
  Index itpc = itpc1;
  Index iwir = ichaRop;
  if ( wrapped && iwir >= nAnchored ) {
    itpc = itpc2;
    iwir -= nAnchored;
  }
  if ( iwir >= nAnchored ) {
    TLOG_ERROR() << __func__ << ": Invalid channel: iwir =" << iwir; exit (1);
  }
  // Loop over wires and create IDs.
  while ( iwir < fWiresPerPlane[icry][itpc][ipla] ) {
    WireID wirid(icry, itpc, ipla, iwir);
    TLOG_DEBUG(4)<<"icha="<<icha<<" pushing: "<<wirid<<" fWiresPerPlane[icry][itpc][ipla]="<<fWiresPerPlane[icry][itpc][ipla];
    wirids.push_back(wirid);
    iwir += fAnchoredWires[icry][itpc][ipla];
    itpc = (itpc == itpc1) ? itpc2 : itpc1;
    TLOG_DEBUG(5)<<"end-of-while iwir="<<iwir<<" itpc="<<itpc<<" fWiresPerPlane="<<fWiresPerPlane[icry][itpc][ipla];
  }
  TLOG_DEBUG_SCOPED(6) {
    TLOG_ADD << "icha="<<icha<<" return wirids.size()="<<wirids.size()<<" [0]="<<wirids[0];
    if (wirids.size()>1) TLOG_ADD << " [1]="<<wirids[1];
  }
  return wirids;
}


int
main(int /*argc*/, char** /*argv*/)
{
  TRACE(TLVL_INFO, "hello");

  DuneApaWireReadoutGeom  readout{};

  auto wire = readout.ChannelToWire(0);

  TLOG() << "wire[0]=" << wire[1];
  return 0;
}
