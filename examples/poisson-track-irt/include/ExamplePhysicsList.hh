// ExamplePhysicsList: G4EmDNAPhysics_option2 + G4EmDNAChemistry_option3 with
// the chemistry time-step model forced to IRT_syn, in pure liquid water.
//
// IRT_syn is G4DNAIndependentReactionTimeModel, built from
// G4DNAIndependentReactionTimeStepper + G4DNAMakeReaction (Geant4 11.4.3,
// G4EmDNAChemistry_option3::ConstructTimeStepModel). It is NOT the classic IRT
// model (G4DNAMolecularIRTModel). The constructor selects IRT_syn through
// G4EmParameters; a macro could still change it with
// /process/chem/TimeStepModel before /run/initialize, so applications check it
// with the two functions below and refuse to run on anything else.
//
// "Pure water, no bulk scavenger": option3's reaction table also lists
// first-order reactions with H2O(B), H3Op(B) and OHm(B). IRT_syn uses those
// only through a G4DNAScavengerMaterial registered with the G4Scheduler; this
// example never registers one, so they stay inert.
#pragma once

#include <memory>
#include <string>

#include "G4VModularPhysicsList.hh"

class G4VPhysicsConstructor;

namespace ptirt
{

class ExamplePhysicsList : public G4VModularPhysicsList
{
 public:
  ExamplePhysicsList();
  ~ExamplePhysicsList() override;

  void ConstructParticle() override;
  void ConstructProcess() override;

 private:
  std::unique_ptr<G4VPhysicsConstructor> fEmDnaPhysics;
  std::unique_ptr<G4VPhysicsConstructor> fEmDnaChemistry;
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

}  // namespace ptirt
