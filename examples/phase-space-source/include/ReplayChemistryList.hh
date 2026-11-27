// ReplayChemistryList: the stage-2 chemistry of phsp-replay (ticket 04).
//
// The PureWater network of PFLASH (PureWaterReactions.cc and
// ChemistryTypes.hh, copied, not linked), the pH acid-base buffer and an
// optional dissolved-O2 bulk scavenger, with the IRT_syn time-step model
// (G4DNAIndependentReactionTimeModel) and no mesoscopic stage. Built like
// PFLASH's DnaChemistryList (the reference for the MT order of the
// scavenger-material install) and examples/dose-rate-study's
// PureWaterChemistryList (the reference for the copy):
//   - molecules and water dissociation: G4ChemDissociationChannels_option1,
//     plus the "H2O" configuration G4DNAScavengerProcess needs;
//   - reaction table: PureWaterReactions::BuildPureWaterReactions, the
//     partially diffusion-controlled pairs as type 1 (IRT_syn);
//   - bulk reactions (BuildPureWaterBulkReactions: the acid-base buffer
//     against H2O, H3Op(B), OHm(B), and e_aq/H/O- + O2): one
//     G4DNAScavengerProcess per molecule and one G4DNAScavengerMaterial per
//     thread, built from ReplayChemistryWorld;
//   - processes: electron solvation if missing, Brownian transport for every
//     molecule but H2O, H2O electron-hole recombination and dissociation
//     (G4DNAWaterDissociationDisplacer), as G4EmDNAChemistry_option3 does.
//
// Bulk reservoir: G4DNAScavengerMaterial holds floor(N_A c V) molecules of
// each bulk species and G4DNAScavengerProcess divides by V again, so V sets
// only the rounding and the depletion. V is a cube of half side
// kBulkReservoirHalfSide (500 um, PFLASH's default water box and the
// dose-rate-study reservoir), not the 4 um box: pH 7 in the box would hold
// floor(3.85) = 3 H3O+. H2O, H3Op(B) and OHm(B) are never depleted
// (G4DNAScavengerMaterial::ReduceNumberMoleculePerVolumeUnitForMaterialConf);
// O2 is, but 21 % (0.27 mM) in V is 1.6e14 molecules, so a history consumes
// a negligible fraction. G4Scheduler resets the material before each
// chemistry stage (fResetScavenger, true by default, G4Scheduler::Process),
// so every history starts from the full bulk.
//
// Where the molecules live: in water only. G4DNABrownianTransportation kills
// a molecule whose step starts outside water (G4DNABrownianTransportation.cc,
// Diffusion, waterDensity == 0), so the box sits in a water margin
// (ReplayDetectorConstruction, /phsp/geom/chemMargin, default 2 um) inside
// the vacuum world: the molecules diffuse freely in box + margin and are
// counted wherever they are. Every Brownian transport of this list carries an
// OutsideWaterAction, which kills such a molecule as Geant4 does and counts
// it (MoleculesKilledOutsideWater, per thread; expected 0). IRT_syn itself
// has no bounding box: the only G4DNABoundingBox here is the bulk reservoir,
// which sets the bulk concentrations (a volume), not where molecules may go.
//
// When it is built: ReplayPhysicsList::EnableChemistry, just before
// /run/initialize and only with /phsp/chem/enable true, so that with false
// nothing of the chemistry exists (no molecule, no G4DNAChemistryManager
// list) and the physics is that of ticket 03. The chemistry manager is
// activated here (SetChemistryList), before the DNA models are initialised:
// G4DNABornIonisationModel1::Initialise keeps the chemistry manager only if
// the chemistry is active then.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "G4SystemOfUnits.hh"
#include "G4VChemistryWorld.hh"
#include "G4VPhysicsConstructor.hh"
#include "G4VUserChemistryList.hh"
#include "globals.hh"

namespace phsp
{

// The chemistry settings of a phsp-replay process (ReplayMessenger,
// /phsp/chem/..., PreInit). One per process, read by every thread.
struct ReplayChemistrySettings
{
  static constexpr G4double kDefaultPH = 7.0;
  static constexpr G4double kDefaultEndTime = 1. * microsecond;
  static constexpr G4double kBulkReservoirHalfSide = 500. * um;
  // Water margin around the box (/phsp/geom/chemMargin), see above.
  static constexpr G4double kDefaultChemMargin = 2. * um;
  // Henry's law for O2 in water at 25 C: a pure-O2 atmosphere gives 1.3 mM,
  // so x % O2 is x / 100 * 1.3 mM (PFLASH ScavengerSpec).
  static constexpr double kO2SaturationMolarity = 0.0013;

  G4bool enabled = true;
  G4double pH = kDefaultPH;
  double o2Molarity = 0;    // mol/L
  std::string o2Text = "0 M";  // as given to /phsp/chem/o2
  G4double chemMargin = kDefaultChemMargin;  // /phsp/geom/chemMargin
};

// The fastest tracked species of the molecule table (the bulk "(B)"
// pseudo-species are never tracked and are skipped): its name and diffusion
// coefficient (Geant4 units). Valid once the molecules exist.
struct FastestSpecies
{
  std::string name;
  G4double diffusionCoefficient = 0;
};
FastestSpecies FindFastestSpecies();

// Molecules killed by G4DNABrownianTransportation because their step started
// outside water, on this thread since the last reset (OutsideWaterAction).
std::uint64_t MoleculesKilledOutsideWater();
void ResetMoleculesKilledOutsideWater();

// Parses "<value> <unit>" with unit M, mM, uM or % (O2 saturation, kH =
// 1.3 mM). Returns false and fills `error` on a bad value, a negative value
// or an unknown unit.
bool ParseO2Concentration(const std::string& text, double& molarity, std::string& error);

// The Species.csv time grid for a chemistry end time: 1 ps x 10^(k/5),
// k = 0, 1, ..., up to the end time, and the end time itself when it is not
// on the grid (5 points per decade; 1 ps is when the chemistry starts).
std::vector<G4double> SpeciesTimeGrid(G4double endTime);
inline constexpr int kSpeciesTimesPerDecade = 5;

class ReplayChemistryWorld : public G4VChemistryWorld
{
 public:
  ReplayChemistryWorld(G4double reservoirHalfSide, G4double pH, double o2Molarity);
  void ConstructChemistryBoundary() override;
  void ConstructChemistryComponents() override;

 private:
  G4double fReservoirHalfSide;
  G4double fPH;
  double fO2Molarity;
};

class ReplayChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor
{
 public:
  explicit ReplayChemistryList(const ReplayChemistrySettings& settings);
  ~ReplayChemistryList() override;

  // G4VUserChemistryList
  void ConstructMolecule() override;
  void ConstructDissociationChannels() override;
  void ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable) override;
  void ConstructTimeStepModel(G4DNAMolecularReactionTable* reactionTable) override;

  // G4VPhysicsConstructor
  void ConstructParticle() override { ConstructMolecule(); }
  void ConstructProcess() override;

  // Whether the Sanche vibrational-excitation model could be extended down
  // to 0.025 eV, as G4EmDNAChemistry_option3 does (master's ConstructProcess).
  // In 11.4.3 G4EmDNABuilder adds the model with AddEmModel, so
  // G4VEmProcess::EmModel() returns null and the extension is not applied,
  // in this example as in option3, UHDR and PFLASH.
  static G4bool SancheExtended();

 private:
  void RegisterBulkReactionProcesses();

  std::unique_ptr<ReplayChemistryWorld> fWorld;
};

}  // namespace phsp
