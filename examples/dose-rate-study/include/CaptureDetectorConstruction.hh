// CaptureDetectorConstruction: a water world with the capture box embedded at
// its centre, both G4_WATER (Geant4-DNA models need G4_WATER).
//
// The capture box is the 4 um cube of the dose-rate study (default half side
// 2 um). The world around it (default half side 10 um, a 20 um cube, so 8 um
// of water on every side, including upstream of the -z entry face) lets delta
// rays born outside the box come back in, and gives the primary an upstream
// path before it enters the box (CaptureRecorder places it at the -z world
// face). Only the energy deposited in the box and the molecules inside it at
// 1 ps make the packet. The box is not a chemistry boundary here: precursors
// are captured before any chemistry step.
//
// Macro commands (PreInit, before /run/initialize):
//   /drs/capture/boxHalfSide <value> <unit>    half side of the capture box, > 0 (default 2 um)
//   /drs/capture/worldHalfSide <value> <unit>  half side of the world, > box (default 10 um)
#pragma once

#include <memory>

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

class G4VPhysicalVolume;

namespace drs
{

class CaptureDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  CaptureDetectorConstruction();
  ~CaptureDetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;

  void SetBoxHalfSide(G4double halfSide);
  void SetWorldHalfSide(G4double halfSide);
  G4double GetBoxHalfSide() const { return fBoxHalfSide; }
  G4double GetWorldHalfSide() const { return fWorldHalfSide; }
  // nullptr before Construct().
  const G4VPhysicalVolume* GetBoxVolume() const { return fBoxVolume; }

 private:
  class Messenger;
  G4double fBoxHalfSide;
  G4double fWorldHalfSide;
  G4VPhysicalVolume* fBoxVolume = nullptr;
  std::unique_ptr<Messenger> fMessenger;
};

}  // namespace drs
