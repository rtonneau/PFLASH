// ReplayDetectorConstruction: the stage-2 geometry of phsp-replay.
//
//   world   G4_Galactic cube, side = box side + 2 x kWorldMargin; nothing is
//           tracked in it (ReplaySteppingAction kills every track there)
//   box     G4_WATER cube of the phase-space files' box side, centred at the
//           origin, unrotated: the stage-1 box frame (origin at the box centre,
//           phantom axes). It is the region "DnaRegion" (ReplayPhysicsList),
//           with the stage-1 region cuts (RecordPhysicsList::SetDnaRegionCuts).
//
// The box is its own region so that stage 2 can use the stage-1 physics setup
// (option4 + DNA_Opt2 activated in a region) unchanged: the world region
// cannot take region-specific DNA models. Any particle leaving the box is
// killed at the box face, so the world only holds the box.
//
// The box side comes from the files (ReplaySource::Open, called from
// Construct at /run/initialize); /phsp/geom/boxSide, if given, must equal it
// (FatalException otherwise). Owns the ReplayMessenger (/phsp/source/...,
// /phsp/geom/boxSide), as RecordDetectorConstruction owns RecordMessenger.
#pragma once

#include <memory>

#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"

class G4LogicalVolume;

namespace phsp
{

class ReplayMessenger;
class ReplaySource;

class ReplayDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  static constexpr const char* kDnaRegionName = "DnaRegion";
  static constexpr G4double kWorldMargin = 1. * um;

  explicit ReplayDetectorConstruction(ReplaySource& source);
  ~ReplayDetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;

  // The box logical volume; null before Construct().
  const G4LogicalVolume* BoxLogical() const { return fBoxLogical; }

 private:
  ReplaySource& fSource;
  std::unique_ptr<ReplayMessenger> fMessenger;
  G4LogicalVolume* fBoxLogical = nullptr;
};

}  // namespace phsp
