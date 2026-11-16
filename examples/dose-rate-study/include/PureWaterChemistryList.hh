// PureWaterChemistryList: the chemical stage of the dose-rate study, the
// project's PureWater Chemistry (copied builder, PureWaterReactions.cc) at a
// fixed pH, with the IRT_syn time-step model and no mesoscopic stage.
//
// Built the same way as the main project's DnaChemistryList (the reference
// for this copy), without its messengers, Chemistry registry or meso stage:
//   - molecules and water dissociation: G4ChemDissociationChannels_option1
//     (the same set as G4EmDNAChemistry_option3), plus an "H2O" configuration
//     that G4DNAScavengerProcess needs;
//   - reaction table: PureWaterReactions::BuildPureWaterReactions, with the
//     partially diffusion-controlled pairs as type 1 (IRT_syn);
//   - bulk reactions (acid-base buffer against H2O, H3Op(B), OHm(B), and the
//     dissolved-O2 reactions, inert here because the bulk holds no O2): one
//     G4DNAScavengerProcess per molecule plus a G4DNAScavengerMaterial built
//     from PureWaterChemistryWorld and installed in the G4Scheduler before the
//     time-step model exists (G4DNAMakeReaction caches it in its constructor).
//   - processes: electron solvation, the Sanche vibrational-excitation
//     extension, Brownian transport for every molecule but H2O, H2O
//     electron-hole recombination and dissociation, as option3 does.
//
// Bulk reservoir volume: G4DNAScavengerMaterial holds floor(N_A c V) bulk
// molecules and G4DNAScavengerProcess divides by V again. In the 4 um box this
// study tracks, pH 7 gives floor(3.85) = 3 H3O+ and OH-, a 22 % low buffer
// rate. Neither class uses the volume for anything else, and H3Op(B), OHm(B)
// and H2O(B) are never depleted (G4DNAScavengerMaterial::Reduce... ignores
// them), so the bulk is given its own large reservoir volume (default half
// side 500 um, the main project's water box): the buffer then acts at the
// nominal concentrations. A bulk species that can be depleted (an O2
// scavenger, for example) would need the tracked volume instead; this study
// has none.
#pragma once

#include <memory>

#include "G4VChemistryWorld.hh"
#include "G4VPhysicsConstructor.hh"
#include "G4VUserChemistryList.hh"
#include "globals.hh"

namespace drs
{

class PureWaterChemistryWorld : public G4VChemistryWorld
{
 public:
  PureWaterChemistryWorld(G4double reservoirHalfSide, G4double pH);
  void ConstructChemistryBoundary() override;
  void ConstructChemistryComponents() override;

  G4double GetReservoirHalfSide() const { return fReservoirHalfSide; }
  G4double GetPH() const { return fPH; }

 private:
  G4double fReservoirHalfSide;
  G4double fPH;
};

class PureWaterChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor
{
 public:
  static constexpr G4double kDefaultPH = 7.0;

  // reservoirHalfSide: half side of the bulk reservoir cube (see above).
  explicit PureWaterChemistryList(G4double pH = kDefaultPH, G4double reservoirHalfSide = 500 * CLHEP::um);
  ~PureWaterChemistryList() override;

  // G4VUserChemistryList
  void ConstructMolecule() override;
  void ConstructDissociationChannels() override;
  void ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable) override;
  void ConstructTimeStepModel(G4DNAMolecularReactionTable* reactionTable) override;

  // G4VPhysicsConstructor
  void ConstructParticle() override { ConstructMolecule(); }
  void ConstructProcess() override;

  const PureWaterChemistryWorld& GetChemistryWorld() const { return *fWorld; }

 private:
  void RegisterBulkReactionProcesses();

  std::unique_ptr<PureWaterChemistryWorld> fWorld;
};

}  // namespace drs
