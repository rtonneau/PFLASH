// ReplayDetectorConstruction: the stage-2 geometry of phsp-replay.
//
//   world   G4_Galactic cube, side = box side + 2 x (chemMargin + kWorldMargin);
//           nothing is tracked in it
//   margin  G4_WATER cube, side = box side + 2 x chemMargin (/phsp/geom/chemMargin,
//           default 2 um; 0 = no margin volume), in the world region: water
//           for the molecules that diffuse out of the box. No physics track
//           ever steps in it: ReplayTrackingAction kills a track that starts
//           outside the box, and ReplaySteppingAction kills a track at the end
//           of the step that leaves the box.
//   box     G4_WATER cube of the phase-space files' box side, centred at the
//           origin, unrotated: the stage-1 box frame (origin at the box centre,
//           phantom axes). It is the region "DnaRegion" (ReplayPhysicsList),
//           with the stage-1 region cuts (RecordPhysicsList::SetDnaRegionCuts);
//           the margin is not in that region.
//
// The box is its own region so that stage 2 can use the stage-1 physics setup
// (option4 + DNA_Opt2 activated in a region) unchanged: the world region
// cannot take region-specific DNA models. Any particle leaving the box is
// killed at the box face, so the world only holds the box.
//
// The box side comes from the files (ReplaySource::Open, called from
// Construct at /run/initialize); /phsp/geom/boxSide, if given, must equal it
// (FatalException otherwise). Owns the ReplayMessenger (/phsp/source/...,
// /phsp/geom/boxSide, /phsp/chem/...), as RecordDetectorConstruction owns
// RecordMessenger.
#pragma once

#include <memory>

#include "G4SystemOfUnits.hh"
#include "G4VUserDetectorConstruction.hh"

class G4LogicalVolume;

namespace phsp
{

class ReplayMessenger;
class ReplaySource;
struct ReplayChemistrySettings;

class ReplayDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  static constexpr const char* kDnaRegionName = "DnaRegion";
  static constexpr G4double kWorldMargin = 1. * um;

  ReplayDetectorConstruction(ReplaySource& source, ReplayChemistrySettings& chemistry);
  ~ReplayDetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;

  // The box logical volume; null before Construct().
  const G4LogicalVolume* BoxLogical() const { return fBoxLogical; }
  // Half side of the box; 0 before Construct().
  G4double BoxHalfSide() const { return fBoxHalfSide; }
  // Water margin around the box used by Construct(); 0 before Construct().
  G4double ChemMargin() const { return fChemMargin; }

 private:
  ReplaySource& fSource;
  const ReplayChemistrySettings& fChemistry;
  std::unique_ptr<ReplayMessenger> fMessenger;
  G4LogicalVolume* fBoxLogical = nullptr;
  G4double fBoxHalfSide = 0;
  G4double fChemMargin = 0;
};

}  // namespace phsp
