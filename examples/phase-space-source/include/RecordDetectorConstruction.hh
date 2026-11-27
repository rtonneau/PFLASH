// RecordDetectorConstruction: the stage-1 geometry of phsp-record.
//
//   world     G4_Galactic cube, side = phantom side + 2 x kWorldMargin
//   phantom   G4_WATER cube of side phantomSide, centred at the origin; the
//             beam enters through its -z face (z = -phantomSide / 2)
//   shell(s)  G4_WATER cubes of side boxSide + 2 x shell, daughters of the
//             phantom, centred on the box centres at depth `depth` below the
//             -z face; the region "DnaRegion" (DNA_Opt2, RecordPhysicsList).
//             Pitch 0: one shell on the axis, copy number 0. Pitch P > 0: shell
//             (ix, iy) at (ix P, iy P) for |ix|, |iy| <= n,
//             n = floor((phantomSide - shellSide) / (2 P)) (every shell inside
//             the phantom), copy number = box index (iy + n)(2n + 1) + (ix + n).
//   box       G4_WATER cube of side boxSide, the one daughter of the shell
//             logical volume, at its centre (copy number 0). The box index of
//             a point in a box is the copy number of its mother shell
//             (touchable depth 1).
//
// Only the shells are on DNA physics; the water between them is condensed
// history. Frames: the global frame is the phantom frame (the phantom sits at
// the origin, unrotated). The box frame of a record has its origin at the
// centre of the box that was entered and the phantom's axes (LatticeFold.hh).
//
// The five parameters are set by the PreInit commands of RecordMessenger
// (/phsp/geom/...). Problem() checks them together; Construct() refuses a
// geometry with a problem (G4Exception), and phsp-record checks it before
// /run/initialize so a bad macro exits cleanly.
#pragma once

#include <memory>
#include <string>

#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

class G4LogicalVolume;

namespace phsp
{

class RecordMessenger;

// Defaults chosen from measurements (README.md, "Defaults: shell and pitch"):
// a 1 um shell is about 20 times the range of the lowest-energy delta ray the
// condensed history produces (990 eV, about 0.05 um), and shells of 0.25 to
// 2 um gave the same in-box deposit; a 50 um pitch costs within 15 % of the
// cheapest per history (20 um) with under 1 % of hit primaries in two boxes.
struct RecordGeometry
{
  G4double phantomSide = 5. * mm;
  G4double depth = 1. * mm;           // of the box centre, below the -z face
  G4double boxSide = 4. * um;
  G4double shell = 1. * um;           // DNA water around the box, each side
  G4double latticePitch = 50. * um;   // 0 = single box; else > boxSide + 2 x shell
};

// Empty when the geometry is consistent, else what is wrong: a non-finite or
// non-positive size, a negative shell, a pitch in (0, boxSide + 2 x shell]
// (neighbouring shells would overlap or touch; with shell 0 a pitch equal to
// the box side makes neighbouring boxes share a face), a shell that does not
// fit in the phantom, or more than kMaxBoxes boxes.
std::string RecordGeometryProblem(const RecordGeometry& geometry);

class RecordDetectorConstruction : public G4VUserDetectorConstruction
{
 public:
  static constexpr const char* kDnaRegionName = "DnaRegion";
  static constexpr G4double kWorldMargin = 0.1 * mm;
  static constexpr G4int kMaxBoxes = 1000000;

  RecordDetectorConstruction();
  ~RecordDetectorConstruction() override;

  G4VPhysicalVolume* Construct() override;

  // PreInit setters (RecordMessenger).
  void SetPhantomSide(G4double value) { fGeometry.phantomSide = value; }
  void SetDepth(G4double value) { fGeometry.depth = value; }
  void SetBoxSide(G4double value) { fGeometry.boxSide = value; }
  void SetShell(G4double value) { fGeometry.shell = value; }
  void SetLatticePitch(G4double value) { fGeometry.latticePitch = value; }

  const RecordGeometry& Geometry() const { return fGeometry; }
  std::string Problem() const { return RecordGeometryProblem(fGeometry); }

  // n: boxes run over ix, iy = -n..n (0 for pitch 0). Set by Construct().
  G4int HalfCount() const { return fHalfCount; }
  G4int BoxCount() const { return (2 * HalfCount() + 1) * (2 * HalfCount() + 1); }
  // z of the box centres in the global (phantom) frame.
  G4double BoxCentreZ() const { return -0.5 * fGeometry.phantomSide + fGeometry.depth; }
  // Side of a DNA shell cube (box + shell on each side).
  G4double ShellSide() const { return fGeometry.boxSide + 2. * fGeometry.shell; }
  // Lattice indices (ix, iy) of the box with this copy number (after Construct()).
  void BoxIndices(G4int copyNumber, G4int& ix, G4int& iy) const;
  // Centre of the box with this copy number, in the global frame (after Construct()).
  G4ThreeVector BoxCentre(G4int copyNumber) const;

  // The logical volume shared by every scoring box; null before Construct().
  const G4LogicalVolume* ScoringBoxLogical() const { return fBoxLogical; }

 private:
  RecordGeometry fGeometry;
  std::unique_ptr<RecordMessenger> fMessenger;
  G4LogicalVolume* fBoxLogical = nullptr;
  G4int fHalfCount = 0;
};

}  // namespace phsp
