// DrsPhysicsList: G4EmDNAPhysics_option2 + PureWaterChemistryList (the
// project's PureWater network at pH 7, IRT_syn, no mesoscopic stage), in
// liquid water.
//
// IRT_syn is G4DNAIndependentReactionTimeModel, built from
// G4DNAIndependentReactionTimeStepper + G4DNAMakeReaction (Geant4 11.4.3). It
// is NOT the classic IRT model (G4DNAMolecularIRTModel). The constructor
// selects IRT_syn through G4EmParameters (PureWaterChemistryList always
// registers the IRT_syn model, and G4DNAElectronSolvation reads the
// parameter); applications check it with the two functions below and refuse
// to run on anything else.
//
// Optional reflecting chemistry box (used by `history`, halfBoxUm > 0): a cube
// of half side reflectingHalfSide centred on the origin. After the chemistry
// has built its processes, the G4DNABrownianTransportation of every molecule
// is replaced by a G4ChemReboundTransportation bound to the box, and the H2O
// dissociation gets a HistoryBoundedBrownianAction, as in the Geant4-DNA UHDR
// example (EmDNAChemistry.cc; ADR 0011). With the default 0 nothing is
// changed, so `capture` builds the free chemistry.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "G4VModularPhysicsList.hh"
#include "globals.hh"

class G4DNABoundingBox;
class G4VPhysicsConstructor;
class G4VProcess;

namespace drs
{

class HistoryBoundedBrownianAction;
class PureWaterChemistryList;

class DrsPhysicsList : public G4VModularPhysicsList
{
 public:
  explicit DrsPhysicsList(G4double reflectingHalfSide = 0);
  ~DrsPhysicsList() override;

  void ConstructParticle() override;
  void ConstructProcess() override;

  // nullptr when the chemistry is unbounded (reflectingHalfSide = 0).
  const G4DNABoundingBox* GetReflectingBox() const { return fBox.get(); }
  // Dissociation products folded back into the box so far (0 when unbounded).
  std::size_t GetDissociationFoldCount() const;

 private:
  void InstallReflectingBox();

  std::unique_ptr<G4VPhysicsConstructor> fEmDnaPhysics;
  std::unique_ptr<PureWaterChemistryList> fChemistry;
  std::unique_ptr<G4DNABoundingBox> fBox;
  // The replaced G4DNABrownianTransportation processes: removed from their
  // process managers, and Geant4 never deletes a transport process, so they are
  // owned here.
  std::vector<std::unique_ptr<G4VProcess>> fRemovedProcesses;
  // Owned by the H2O dissociation process.
  HistoryBoundedBrownianAction* fDissociationAction = nullptr;
};

// Empty when G4EmParameters selects IRT_syn and the Geant4-DNA chemistry is
// active; otherwise a message naming what is wrong. Valid after /run/initialize.
std::string DescribeIrtSynParameterProblem();

// Empty when the G4Scheduler holds exactly one active time-step model at
// globalTime and it is G4DNAIndependentReactionTimeModel with a
// G4DNAIndependentReactionTimeStepper and a G4DNAMakeReaction; otherwise a
// message. In Serial mode the model is registered when the chemistry first
// runs, so call it from G4UserTimeStepAction::StartProcessing or later.
std::string DescribeIrtSynSchedulerProblem(double globalTime);

}  // namespace drs
