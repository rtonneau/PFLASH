#include "RecordDetectorConstruction.hh"

#include <cmath>
#include <sstream>

#include "LatticeFold.hh"
#include "RecordMessenger.hh"
#include "RecordPhysicsList.hh"

#include "G4Box.hh"
#include "G4Exception.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4Region.hh"

namespace phsp
{

namespace
{

G4String Length(G4double value)
{
  std::ostringstream text;
  text.precision(12);
  text << value / um << " um";
  return text.str();
}

// n of a lattice of this pitch: every shell (|ix|, |iy| <= n) lies inside the
// phantom laterally. Assumes a consistent pitch > 0.
G4double LatticeHalfCount(const RecordGeometry& geom)
{
  const G4double shellSide = geom.boxSide + 2. * geom.shell;
  return std::floor((geom.phantomSide - shellSide) / (2. * geom.latticePitch));
}

}  // namespace

std::string RecordGeometryProblem(const RecordGeometry& geom)
{
  const auto positive = [](G4double v) { return std::isfinite(v) && v > 0; };
  if (!positive(geom.phantomSide)) return "phantomSide must be finite and > 0";
  if (!positive(geom.depth)) return "depth must be finite and > 0";
  if (!positive(geom.boxSide)) return "boxSide must be finite and > 0";
  if (!std::isfinite(geom.shell) || geom.shell < 0) return "shell must be finite and >= 0";
  if (!std::isfinite(geom.latticePitch) || geom.latticePitch < 0) return "latticePitch must be finite and >= 0";
  const G4double shellSide = geom.boxSide + 2. * geom.shell;
  if (geom.latticePitch > 0 && geom.latticePitch <= shellSide)
    return "latticePitch (" + Length(geom.latticePitch) +
           ") must be 0 (single box) or greater than boxSide + 2 x shell (" + Length(shellSide) +
           "): otherwise neighbouring DNA shells overlap or share a face (with shell 0, neighbouring boxes share "
           "a face)";
  const G4double halfShell = 0.5 * shellSide;
  if (geom.depth - halfShell <= 0 || geom.depth + halfShell >= geom.phantomSide)
    return "the DNA shell (depth " + Length(geom.depth) + " +/- " + Length(halfShell) +
           ") must lie strictly inside the phantom (side " + Length(geom.phantomSide) + ")";
  if (shellSide >= geom.phantomSide)
    return "the DNA shell (" + Length(shellSide) + " wide) must be narrower than the phantom (" +
           Length(geom.phantomSide) + ")";
  if (geom.latticePitch > 0)
  {
    const G4double side = 2. * LatticeHalfCount(geom) + 1.;
    if (side * side > RecordDetectorConstruction::kMaxBoxes)
      return "the lattice would hold " + std::to_string(static_cast<long long>(side * side)) + " boxes (more than " +
             std::to_string(RecordDetectorConstruction::kMaxBoxes) + "); use a larger latticePitch";
  }
  return {};
}

RecordDetectorConstruction::RecordDetectorConstruction() : fMessenger(std::make_unique<RecordMessenger>(this)) {}

RecordDetectorConstruction::~RecordDetectorConstruction() = default;

void RecordDetectorConstruction::BoxIndices(G4int copyNumber, G4int& ix, G4int& iy) const
{
  const G4int side = 2 * fHalfCount + 1;
  ix = copyNumber % side - fHalfCount;
  iy = copyNumber / side - fHalfCount;
}

G4ThreeVector RecordDetectorConstruction::BoxCentre(G4int copyNumber) const
{
  G4int ix = 0, iy = 0;
  BoxIndices(copyNumber, ix, iy);
  const auto centre = LatticeFold(fGeometry.latticePitch, BoxCentreZ()).BoxCentre(ix, iy);
  return {centre[0], centre[1], centre[2]};
}

G4VPhysicalVolume* RecordDetectorConstruction::Construct()
{
  if (const auto problem = Problem(); !problem.empty())
    G4Exception("RecordDetectorConstruction::Construct", "PhspGeom001", FatalErrorInArgument, problem.c_str());

  fHalfCount = fGeometry.latticePitch > 0 ? static_cast<G4int>(LatticeHalfCount(fGeometry)) : 0;

  auto* nist = G4NistManager::Instance();
  G4Material* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* water = nist->FindOrBuildMaterial("G4_WATER");

  const G4double halfPhantom = 0.5 * fGeometry.phantomSide;
  const G4double halfWorld = halfPhantom + kWorldMargin;
  auto* worldSolid = new G4Box("World", halfWorld, halfWorld, halfWorld);
  auto* worldLogical = new G4LogicalVolume(worldSolid, vacuum, "World");
  auto* worldPhysical = new G4PVPlacement(nullptr, {}, worldLogical, "World", nullptr, false, 0, true);

  auto* phantomSolid = new G4Box("Phantom", halfPhantom, halfPhantom, halfPhantom);
  auto* phantomLogical = new G4LogicalVolume(phantomSolid, water, "Phantom");
  new G4PVPlacement(nullptr, {}, phantomLogical, "Phantom", worldLogical, false, 0, true);

  const G4double halfShell = 0.5 * ShellSide();
  auto* shellSolid = new G4Box("DnaShell", halfShell, halfShell, halfShell);
  auto* shellLogical = new G4LogicalVolume(shellSolid, water, "DnaShell");

  auto* region = new G4Region(kDnaRegionName);
  region->AddRootLogicalVolume(shellLogical);
  RecordPhysicsList::SetDnaRegionCuts(region);

  const G4double halfBox = 0.5 * fGeometry.boxSide;
  auto* boxSolid = new G4Box("ScoringBox", halfBox, halfBox, halfBox);
  auto* boxLogical = new G4LogicalVolume(boxSolid, water, "ScoringBox");
  new G4PVPlacement(nullptr, {}, boxLogical, "ScoringBox", shellLogical, false, 0, true);

  const G4int count = BoxCount();
  for (G4int copy = 0; copy < count; ++copy)
  {
    // Overlap check on the first and last copies only (the lattice is regular).
    const G4bool check = copy == 0 || copy == count - 1;
    new G4PVPlacement(nullptr, BoxCentre(copy), shellLogical, "DnaShell", phantomLogical, false, copy, check);
  }
  fBoxLogical = boxLogical;
  return worldPhysical;
}

}  // namespace phsp
