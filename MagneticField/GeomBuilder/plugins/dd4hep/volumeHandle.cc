// #include "Utilities/Configuration/interface/Architecture.h"

/*
 *  See header file for a description of this class.
 *
 *  \author N. Amapane - INFN Torino
 */

#include "MagneticField/GeomBuilder/plugins/dd4hep/volumeHandle.h"

#include "DataFormats/GeometrySurface/interface/Plane.h"
#include "DataFormats/GeometrySurface/interface/Cylinder.h"
#include "DataFormats/GeometrySurface/interface/Cone.h"
#include "DataFormats/GeometryVector/interface/CoordinateSets.h"
#include "DataFormats/Math/interface/GeantUnits.h"
#include "DataFormats/Math/interface/Vector3D.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DetectorDescription/DDCMS/interface/DDShapes.h"

#include "MagneticField/Layers/interface/MagVerbosity.h"

#include <string>
#include <iterator>
#include <iomanip>
#include <iostream>
#include <boost/lexical_cast.hpp>

using namespace SurfaceOrientation;
using namespace std;
using namespace magneticfield;
using namespace cms;
using namespace cms::dd;
using namespace edm;

volumeHandle::volumeHandle(const DDFilteredView &fv, bool expand2Pi, bool debugVal)
    : BaseVolumeHandle(debugVal), shape_(getCurrentShape(fv)), solid(fv) {
  name = fv.name();
  copyno = fv.copyNum();
  expand = expand2Pi;
  const Double_t *const transArray = fv.trans();
  center_ = GlobalPoint(transArray[0], transArray[1], transArray[2]);

  // ASSUMPTION: volume names ends with "_NUM" where NUM is the volume number
  string volName = name;
  volName.erase(0, volName.rfind('_') + 1);
  volumeno = boost::lexical_cast<unsigned short>(volName);

  for (int i = 0; i < 6; ++i) {
    isAssigned[i] = false;
  }
  referencePlane(fv);
  switch (shape_) {
    case DDSolidShape::ddbox:
      buildBox();
      break;
    case DDSolidShape::ddtrap:
      buildTrap();
      break;
    case DDSolidShape::ddcons:
      buildCons();
      break;
    case DDSolidShape::ddtubs:
      buildTubs();
      break;
    case DDSolidShape::ddtrunctubs:
      buildTruncTubs();
      break;
    default:
      LogError("magneticfield::volumeHandle")
          << "ctor: Unexpected shape_ # " << static_cast<int>(shape_) << " for vol " << name;
  }

  // Get material for this volume
  if (fv.materialName() == "Iron")
    isIronFlag = true;

  if (debug) {
    LogDebug("magneticfield::volumeHandle") << " RMin =  " << theRMin;
    LogDebug("magneticfield::volumeHandle") << " RMax =  " << theRMax;

    if (theRMin < 0 || theRN < theRMin || theRMax < theRN)
      LogDebug("magneticfield::volumeHandle") << "*** WARNING: wrong RMin/RN/RMax";

    LogDebug("magneticfield::volumeHandle")
        << "Summary: " << name << " " << copyno << " shape = " << shape_ << " trasl " << center() << " R "
        << center().perp() << " phi " << center().phi() << " magFile " << magFile << " Material= " << fv.materialName()
        << " isIron= " << isIronFlag << " masterSector= " << masterSector << std::endl;

    LogDebug("magneticfield::volumeHandle") << " Orientation of surfaces:";
    std::string sideName[3] = {"positiveSide", "negativeSide", "onSurface"};
    for (int i = 0; i < 6; ++i) {
      if (surfaces[i] != nullptr)
        LogDebug("magneticfield::volumeHandle") << "  " << i << ":" << sideName[surfaces[i]->side(center_, 0.3)];
    }
  }
}

void volumeHandle::referencePlane(const DDFilteredView &fv) {
  // The refPlane is the "main plane" for the solid. It corresponds to the
  // x,y plane in the DDD local frame, and defines a frame where the local
  // coordinates are the same as in DDD.
  // In the geometry version 85l_030919, this plane is normal to the
  // beam line for all volumes but pseudotraps, so that global R is along Y,
  // global phi is along -X and global Z along Z:
  //
  //   Global(for vol at pi/2)    Local
  //   +R (+Y)                    +Y
  //   +phi(-X)                   -X
  //   +Z                         +Z
  //
  // For pseudotraps the refPlane is parallel to beam line and global R is
  // along Z, global phi is along +-X and and global Z along Y:
  //
  //   Global(for vol at pi/2)    Local
  //   +R (+Y)                    +Z
  //   +phi(-X)                   +X
  //   +Z                         +Y
  //
  // Note that the frame is centered in the DDD volume center, which is
  // inside the volume for DDD boxes and (pesudo)trapezoids, on the beam line
  // for tubs, cons and trunctubs.

  // In geometry version 1103l, trapezoids have X and Z in the opposite direction
  // than the above.  Boxes are either oriented as described above or in some case
  // have opposite direction for Y and X.

  // The global position
  Surface::PositionType &posResult = center_;

  // The reference plane rotation
  math::XYZVector x, y, z;
  dd4hep::Rotation3D refRot;
  fv.rot(refRot);
  refRot.GetComponents(x, y, z);
  if (debug) {
    if (x.Cross(y).Dot(z) < 0.5) {
      LogDebug("magneticfield::volumeHandle") << "*** WARNING: Rotation is not RH ";
    }
  }

  // The global rotation
  Surface::RotationType rotResult(float(x.X()),
                                  float(x.Y()),
                                  float(x.Z()),
                                  float(y.X()),
                                  float(y.Y()),
                                  float(y.Z()),
                                  float(z.X()),
                                  float(z.Y()),
                                  float(z.Z()));

  refPlane = new GloballyPositioned<float>(posResult, rotResult);

  // Check correct orientation
  if (debug) {
    LogDebug("magneticfield::volumeHandle") << "Refplane pos  " << refPlane->position();

    // See comments above for the conventions for orientation.
    LocalVector globalZdir(0., 0., 1.);  // Local direction of the axis along global Z

    /* Preserve in case pseudotrap is needed again
    if (shape_ == DDSolidShape::ddpseudotrap) {
      globalZdir = LocalVector(0., 1., 0.);
    }
    */
    if (refPlane->toGlobal(globalZdir).z() < 0.) {
      globalZdir = -globalZdir;
    }
    float chk = refPlane->toGlobal(globalZdir).dot(GlobalVector(0, 0, 1));
    if (chk < .999)
      LogDebug("magneticfield::volumeHandle") << "*** WARNING RefPlane check failed!***" << chk;
  }
}

std::vector<VolumeSide> volumeHandle::sides() const {
  std::vector<VolumeSide> result;
  for (int i = 0; i < 6; ++i) {
    // If this is just a master volume out of wich a 2pi volume
    // should be built (e.g. central cylinder), skip the phi boundaries.
    if (expand && (i == phiplus || i == phiminus))
      continue;

    // FIXME: Skip null inner degenerate cylindrical surface
    if (shape_ == DDSolidShape::ddtubs && i == SurfaceOrientation::inner && theRMin < 0.001)
      continue;

    ReferenceCountingPointer<Surface> s = const_cast<Surface *>(surfaces[i].get());
    result.push_back(VolumeSide(s, GlobalFace(i), surfaces[i]->side(center_, 0.3)));
  }
  return result;
}

// To allow the following code to work with both old and new DD,
// we need this function that merely returns its argument.
// For the old DD, another version of this function converts mm to cm.

template <class NumType>
inline constexpr NumType convertUnits(NumType centimeters) {
  return (centimeters);
}

#include "MagneticField/GeomBuilder/src/buildBox.icc"
#include "MagneticField/GeomBuilder/src/buildTrap.icc"
#include "MagneticField/GeomBuilder/src/buildTubs.icc"
#include "MagneticField/GeomBuilder/src/buildCons.icc"
#include "MagneticField/GeomBuilder/src/buildTruncTubs.icc"
