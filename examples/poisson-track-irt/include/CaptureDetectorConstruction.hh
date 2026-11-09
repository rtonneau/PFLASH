// CaptureDetectorConstruction: the capture volume is the whole world, one
// G4_WATER box (Geant4-DNA models need G4_WATER).
//
// Everything a primary deposits is deposited in this box; what leaves it is
// counted as escaped energy (see CaptureRecorder). Its size only decides what
// escapes: precursors are captured before any chemistry, so the box is not a
// chemistry boundary.
//
// Macro command (PreInit, before /run/initialize):
//   /ptirt/capture/halfSide <value> <unit>   half side of the box, > 0 (default 10 um)
#pragma once

#include <memory>

#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

namespace ptirt
{

class CaptureDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  CaptureDetectorConstruction();
  ~CaptureDetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;

  void SetHalfSide(G4double halfSide);
  G4double GetHalfSide() const { return fHalfSide; }

 private:
  class Messenger;
  G4double fHalfSide;
  std::unique_ptr<Messenger> fMessenger;
};

}  // namespace ptirt
