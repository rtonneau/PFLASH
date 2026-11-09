#include "ExamplePhysicsList.hh"

#include "ExamplePhysicsTag.hh"
#include "HistoryBoundedBrownianAction.hh"

#include "G4ChemReboundTransportation.hh"
#include "G4ChemTimeStepModel.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNABrownianTransportation.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAIndependentReactionTimeModel.hh"
#include "G4DNAIndependentReactionTimeStepper.hh"
#include "G4DNAMakeReaction.hh"
#include "G4DNAMolecularDissociation.hh"
#include "G4EmDNAChemistry_option3.hh"
#include "G4EmDNAPhysics_option2.hh"
#include "G4EmParameters.hh"
#include "G4Exception.hh"
#include "G4H2O.hh"
#include "G4ITModelHandler.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicsListHelper.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4ProductionCutsTable.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4VITStepModel.hh"
#include "G4VProcess.hh"
#include "G4Version.hh"

// The compatibility tag names Geant4 11.4.3; packets from another version must
// not carry it.
static_assert(G4VERSION_NUMBER == 1143,
              "ExamplePhysicsTag.hh names Geant4 11.4.3: update kPacketCompatibilityTag");

namespace ptirt
{

ExamplePhysicsList::ExamplePhysicsList(G4double reflectingHalfSide)
  : fEmDnaPhysics(std::make_unique<G4EmDNAPhysics_option2>(/*verbose*/ 0)),
    fEmDnaChemistry(std::make_unique<G4EmDNAChemistry_option3>())
{
  if (!(reflectingHalfSide >= 0))
    G4Exception("ExamplePhysicsList::ExamplePhysicsList", "PTIRT_REFLECTING_BOX", FatalErrorInArgument,
                "the reflecting box half side must be 0 (no box) or positive");
  if (reflectingHalfSide > 0)
  {
    const G4double h = reflectingHalfSide;
    fBox = std::make_unique<G4DNABoundingBox>(G4DNABoundingBox{h, -h, h, -h, h, -h});  // xhi xlo yhi ylo zhi zlo
  }

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
  if (fBox) InstallReflectingBox();
}

std::size_t ExamplePhysicsList::GetDissociationFoldCount() const
{
  return fDissociationAction != nullptr ? fDissociationAction->GetFoldCount() : 0;
}

void ExamplePhysicsList::InstallReflectingBox()
{
  auto* helper = G4PhysicsListHelper::GetPhysicsListHelper();
  auto iterator = G4MoleculeTable::Instance()->GetDefintionIterator();
  iterator.reset();
  while (iterator())
  {
    G4MoleculeDefinition* molecule = iterator.value();
    G4ProcessManager* manager = molecule->GetProcessManager();
    const G4String name = molecule->GetName();
    if (manager == nullptr)
      G4Exception("ExamplePhysicsList::InstallReflectingBox", "PTIRT_REFLECTING_BOX", FatalException,
                  ("molecule " + name + " has no process manager").c_str());
    G4ProcessVector* processes = manager->GetProcessList();

    if (molecule == G4H2O::Definition())
    {
      // H2O does not move; its dissociation products are folded into the box.
      G4DNAMolecularDissociation* dissociation = nullptr;
      for (G4int i = 0; i < static_cast<G4int>(processes->size()); ++i)
        if ((*processes)[i]->GetProcessName() == "H2O_DNAMolecularDecay")
          dissociation = dynamic_cast<G4DNAMolecularDissociation*>((*processes)[i]);
      if (dissociation == nullptr)
        G4Exception("ExamplePhysicsList::InstallReflectingBox", "PTIRT_REFLECTING_BOX", FatalException,
                    "option3's H2O_DNAMolecularDecay process was not found");
      fDissociationAction = new HistoryBoundedBrownianAction(*fBox);
      dissociation->SetUserBrownianAction(fDissociationAction);  // the process owns it
      continue;
    }

    // Every other molecule: swap option3's free Brownian transport for the
    // rebound transport. Both are transportation processes of the same
    // subtype, so the old one is removed before the new one is registered.
    G4VProcess* brownian = nullptr;
    for (G4int i = 0; i < static_cast<G4int>(processes->size()); ++i)
      if (dynamic_cast<G4DNABrownianTransportation*>((*processes)[i]) != nullptr) brownian = (*processes)[i];
    if (brownian == nullptr)
      G4Exception("ExamplePhysicsList::InstallReflectingBox", "PTIRT_REFLECTING_BOX", FatalException,
                  ("molecule " + name + " has no G4DNABrownianTransportation to replace").c_str());
    if (manager->RemoveProcess(brownian) != brownian)
      G4Exception("ExamplePhysicsList::InstallReflectingBox", "PTIRT_REFLECTING_BOX", FatalException,
                  ("cannot remove the Brownian transport of " + name).c_str());
    fRemovedProcesses.emplace_back(brownian);
    if (!helper->RegisterProcess(new G4ChemReboundTransportation("ChemReboundTransportation", fBox.get()),
                                 molecule))
      G4Exception("ExamplePhysicsList::InstallReflectingBox", "PTIRT_REFLECTING_BOX", FatalException,
                  ("cannot register the rebound transport of " + name).c_str());
  }
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
