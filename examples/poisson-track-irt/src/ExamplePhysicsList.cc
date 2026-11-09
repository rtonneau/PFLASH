#include "ExamplePhysicsList.hh"

#include "ExamplePhysicsTag.hh"

#include "G4ChemTimeStepModel.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAIndependentReactionTimeModel.hh"
#include "G4DNAIndependentReactionTimeStepper.hh"
#include "G4DNAMakeReaction.hh"
#include "G4EmDNAChemistry_option3.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmParameters.hh"
#include "G4ITModelHandler.hh"
#include "G4ProductionCutsTable.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4VITStepModel.hh"
#include "G4Version.hh"

// The compatibility tag names Geant4 11.4.3; packets from another version must
// not carry it.
static_assert(G4VERSION_NUMBER == 1143,
              "ExamplePhysicsTag.hh names Geant4 11.4.3: update kPacketCompatibilityTag");

namespace ptirt
{

ExamplePhysicsList::ExamplePhysicsList()
  : fEmDnaPhysics(std::make_unique<G4EmDNAPhysics_option2>(/*verbose*/ 0)),
    fEmDnaChemistry(std::make_unique<G4EmDNAChemistry_option3>())
{
  // Same cuts as the Geant4-DNA chemistry examples (chem1, chem6).
  G4ProductionCutsTable::GetProductionCutsTable()->SetEnergyRange(100 * eV, 1 * GeV);
  SetDefaultCutValue(1 * nanometer);
  SetVerboseLevel(0);

  // Force IRT_syn (G4EmDNAChemistry_option3 reads this in ConstructReactionTable,
  // ConstructProcess and ConstructTimeStepModel).
  G4EmParameters::Instance()->SetTimeStepModel(G4ChemTimeStepModel::IRT_syn);
  G4EmParameters::Instance()->SetVerbose(0);
}

ExamplePhysicsList::~ExamplePhysicsList() = default;

void ExamplePhysicsList::ConstructParticle()
{
  fEmDnaPhysics->ConstructParticle();
  fEmDnaChemistry->ConstructParticle();
}

void ExamplePhysicsList::ConstructProcess()
{
  AddTransportation();
  fEmDnaPhysics->ConstructProcess();
  // option3 extends the EM-DNA processes (vibrational excitation, solvation),
  // so it is constructed after them.
  fEmDnaChemistry->ConstructProcess();
}

namespace
{

const char* ModelName(G4ChemTimeStepModel model)
{
  switch (model)
  {
    case G4ChemTimeStepModel::SBS: return "SBS";
    case G4ChemTimeStepModel::IRT: return "IRT (classic G4DNAMolecularIRTModel)";
    case G4ChemTimeStepModel::IRT_syn: return "IRT_syn";
    default: return "Unknown";
  }
}

}  // namespace

std::string DescribeIrtSynParameterProblem()
{
  if (!G4DNAChemistryManager::IsActivated())
    return "the Geant4-DNA chemistry is not active (/chem/activate false?); this example needs it";
  const auto model = G4EmParameters::Instance()->GetTimeStepModel();
  if (model != G4ChemTimeStepModel::IRT_syn)
    return std::string("the chemistry time-step model is ") + ModelName(model) +
           "; this example requires IRT_syn (remove /process/chem/TimeStepModel from the macro)";
  return {};
}

std::string DescribeIrtSynSchedulerProblem(double globalTime)
{
  auto* handler = G4Scheduler::Instance()->GetModelHandler();
  if (handler == nullptr) return "the G4Scheduler has no model handler";
  const auto models = handler->GetActiveModels(globalTime);
  if (models.size() != 1)
    return "expected exactly one active chemistry time-step model, found " +
           std::to_string(models.size());
  auto* model = models.front();
  if (dynamic_cast<G4DNAIndependentReactionTimeModel*>(model) == nullptr)
    return "the active chemistry model '" + model->GetName() +
           "' is not G4DNAIndependentReactionTimeModel (IRT_syn)";
  if (dynamic_cast<G4DNAIndependentReactionTimeStepper*>(model->GetTimeStepper()) == nullptr)
    return "the IRT_syn model does not use G4DNAIndependentReactionTimeStepper";
  if (dynamic_cast<G4DNAMakeReaction*>(model->GetReactionProcess()) == nullptr)
    return "the IRT_syn model does not use G4DNAMakeReaction";
  return {};
}

}  // namespace ptirt
