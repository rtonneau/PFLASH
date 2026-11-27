#include "ReplayPhysicsList.hh"

#include "RecordPhysicsList.hh"
#include "ReplayChemistryList.hh"
#include "ReplayDetectorConstruction.hh"

#include "G4ChemTimeStepModel.hh"
#include "G4EmDNAPhysicsActivator.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmParameters.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4Exception.hh"
#include "G4ParticleTable.hh"

namespace phsp
{

const char* ReplayPhysicsName(ReplayPhysics physics)
{
  return physics == ReplayPhysics::kStage1 ? "stage1" : "option2";
}

const char* ReplayPhysicsDescription(ReplayPhysics physics)
{
  return physics == ReplayPhysics::kStage1
             ? "stage-1 setup: G4EmStandardPhysics_option4 + DNA_Opt2 in DnaRegion (the box) via "
               "G4EmParameters::AddDNA + G4EmDNAPhysicsActivator, stage-1 cuts"
             : "G4EmDNAPhysics_option2 constructor everywhere, stage-1 cuts";
}

ReplayPhysicsList::ReplayPhysicsList(ReplayPhysics physics) : fPhysics(physics)
{
  SetDefaultCutValue(RecordPhysicsList::kDefaultCut);
  if (physics == ReplayPhysics::kStage1)
  {
    // Same order as RecordPhysicsList: option4's constructor resets
    // G4EmParameters (SetDefaults), which would drop a DNA region added before.
    RegisterPhysics(new G4EmStandardPhysics_option4());
    G4EmParameters::Instance()->AddDNA(ReplayDetectorConstruction::kDnaRegionName, RecordPhysicsList::kDnaOption);
    RegisterPhysics(new G4EmDNAPhysicsActivator());
  }
  else
  {
    RegisterPhysics(new G4EmDNAPhysics_option2());
  }
}

void ReplayPhysicsList::EnableChemistry(const ReplayChemistrySettings& settings)
{
  if (fChemistry != nullptr) return;
  auto* parameters = G4EmParameters::Instance();
  const G4ChemTimeStepModel model = parameters->GetTimeStepModel();
  if (model == G4ChemTimeStepModel::Unknown)
    parameters->SetTimeStepModel(G4ChemTimeStepModel::IRT_syn);
  else if (model != G4ChemTimeStepModel::IRT_syn)
  {
    G4Exception("ReplayPhysicsList::EnableChemistry", "PhspChem010", FatalException,
                "/process/chem/TimeStepModel: phsp-replay runs IRT_syn only (remove the command)");
    return;
  }

  // The molecule definitions created by ConstructParticle get no definition
  // ID from SetupPhysics, which ran before; give them one, as SetupPhysics
  // does for every particle that is not a general ion.
  fChemistry = new ReplayChemistryList(settings);
  fChemistry->ConstructParticle();
  auto* iterator = G4ParticleTable::GetParticleTable()->GetIterator();
  iterator->reset();
  while ((*iterator)())
  {
    G4ParticleDefinition* particle = iterator->value();
    if (!particle->IsGeneralIon() && particle->GetInstanceID() < 0) particle->SetParticleDefinitionID();
  }
  // Last: its ConstructProcess extends the e- processes of the EM constructors.
  RegisterPhysics(fChemistry);
}

}  // namespace phsp
