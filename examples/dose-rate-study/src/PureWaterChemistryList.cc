#include "PureWaterChemistryList.hh"

#include "ChemistryTypes.hh"
#include "PureWaterReactions.hh"

#include "G4ChemDissociationChannels_option1.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNABrownianTransportation.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAElectronHoleRecombination.hh"
#include "G4DNAElectronSolvation.hh"
#include "G4DNAIndependentReactionTimeModel.hh"
#include "G4DNAMolecularDissociation.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4DNASancheExcitationModel.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4DNAScavengerProcess.hh"
#include "G4DNAVibExcitation.hh"
#include "G4DNAWaterDissociationDisplacer.hh"
#include "G4Electron.hh"
#include "G4Exception.hh"
#include "G4H2O.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicsListHelper.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessTable.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>

namespace drs
{

namespace
{

const G4MolecularConfiguration* Conf(const G4String& name)
{
  auto* configuration = G4MoleculeTable::Instance()->GetConfiguration(name, false);
  if (configuration == nullptr)
    G4Exception("PureWaterChemistryList", "DRS_MISSING_SPECIES", FatalException,
                ("unknown species configuration: " + name).c_str());
  return configuration;
}

// Same conversion as the main project's DnaChemistryList (MakeBulkReactionData).
G4DNAMolecularReactionData* MakeBulkReactionData(const G4MolecularConfiguration* molecule,
                                                 const ChemistryTypes::BulkReaction& reaction)
{
  auto* data = new G4DNAMolecularReactionData(reaction.rate, molecule, Conf(reaction.partner));
  for (const auto& product : reaction.products) data->AddProduct(Conf(product));
  if (reaction.reactionType != 0 &&
      (reaction.reactionType != 1 || ChemistryTypes::PartialReactionsEnabled()))
    data->SetReactionType(reaction.reactionType);
  return data;
}

}  // namespace

PureWaterChemistryWorld::PureWaterChemistryWorld(G4double reservoirHalfSide, G4double pH)
  : fReservoirHalfSide(reservoirHalfSide), fPH(pH)
{
  if (!(reservoirHalfSide > 0))
    G4Exception("PureWaterChemistryWorld", "DRS_RESERVOIR", FatalErrorInArgument,
                "the bulk reservoir half side must be positive");
  if (!(pH > 0 && pH < 14))
    G4Exception("PureWaterChemistryWorld", "DRS_PH", FatalErrorInArgument, "pH must be in (0, 14)");
  ConstructChemistryBoundary();
}

void PureWaterChemistryWorld::ConstructChemistryBoundary()
{
  const G4double h = fReservoirHalfSide;
  // G4DNABoundingBox order: xhi xlo yhi ylo zhi zlo.
  fpChemistryBoundary = std::make_unique<G4DNABoundingBox>(G4DNABoundingBox{h, -h, h, -h, h, -h});
}

void PureWaterChemistryWorld::ConstructChemistryComponents()
{
  // Constants of the Geant4 UHDR ChemistryWorld, as in the main project's
  // DnaChemistryWorld. No O2 and no other scavenger.
  const G4double pKw = 14.0;
  const G4double waterConcentration_M = 55.3;
  fpChemicalComponent.clear();
  fpChemicalComponent[Conf("H2O(B)")] = waterConcentration_M / (mole * liter);
  fpChemicalComponent[Conf("H3Op(B)")] = std::pow(10.0, -fPH) / (mole * liter);
  fpChemicalComponent[Conf("OHm(B)")] = std::pow(10.0, -(pKw - fPH)) / (mole * liter);
}

PureWaterChemistryList::PureWaterChemistryList(G4double pH, G4double reservoirHalfSide)
  : G4VUserChemistryList(true),
    fWorld(std::make_unique<PureWaterChemistryWorld>(reservoirHalfSide, pH))
{
  // The flag above marks this object as a G4VPhysicsConstructor, so the
  // chemistry manager releases it instead of deleting it (the physics list
  // owns it).
  G4DNAChemistryManager::Instance()->SetChemistryList(this);
  // IRT_syn: the partially diffusion-controlled pairs are type 1. Set before
  // any builder runs.
  ChemistryTypes::SetPartialReactionsEnabled(true);
}

PureWaterChemistryList::~PureWaterChemistryList() = default;

void PureWaterChemistryList::ConstructMolecule()
{
  G4ChemDissociationChannels_option1::ConstructMolecule();
  // Required by G4DNAScavengerProcess (member init: GetConfiguration("H2O")).
  if (G4MoleculeTable::Instance()->GetConfiguration("H2O", false) == nullptr)
    G4MoleculeTable::Instance()->CreateConfiguration("H2O", G4H2O::Definition());
}

void PureWaterChemistryList::ConstructDissociationChannels()
{
  G4ChemDissociationChannels_option1::ConstructDissociationChannels();
}

void PureWaterChemistryList::ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable)
{
  // The bulk composition is set here, before the G4DNAScavengerMaterial reads it.
  fWorld->ConstructChemistryComponents();
  // No mesoscopic stage, so the bulk reactions stay out of the reaction table
  // (the particle-based stage uses them only through G4DNAScavengerProcess).
  PureWaterReactions::BuildPureWaterReactions(reactionTable);
}

void PureWaterChemistryList::ConstructTimeStepModel(G4DNAMolecularReactionTable* /*reactionTable*/)
{
  RegisterTimeStepModel(new G4DNAIndependentReactionTimeModel(), 0);
}

void PureWaterChemistryList::ConstructProcess()
{
  auto* helper = G4PhysicsListHelper::GetPhysicsListHelper();

  // Extend the Sanche vibrational-excitation model down to thermal energies.
  if (auto* vib = dynamic_cast<G4DNAVibExcitation*>(
          G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAVibExcitation", "e-")))
    if (auto* sanche = dynamic_cast<G4DNASancheExcitationModel*>(vib->EmModel()))
      sanche->ExtendLowEnergyLimit(0.025 * eV);

  if (G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAElectronSolvation", "e-") == nullptr)
    helper->RegisterProcess(new G4DNAElectronSolvation("e-_G4DNAElectronSolvation"),
                            G4Electron::Definition());

  auto iterator = G4MoleculeTable::Instance()->GetDefintionIterator();
  iterator.reset();
  while (iterator())
  {
    G4MoleculeDefinition* molecule = iterator.value();
    if (molecule != G4H2O::Definition())
    {
      helper->RegisterProcess(new G4DNABrownianTransportation(), molecule);
    }
    else
    {
      molecule->GetProcessManager()->AddRestProcess(new G4DNAElectronHoleRecombination(), 2);
      auto* dissociation = new G4DNAMolecularDissociation("H2O_DNAMolecularDecay");
      dissociation->SetDisplacer(molecule, new G4DNAWaterDissociationDisplacer);
      molecule->GetProcessManager()->AddRestProcess(dissociation, 1);
    }
  }

  RegisterBulkReactionProcesses();

  // Serial only. Initialize() runs ConstructReactionTable (bulk composition);
  // the time-step model, whose G4DNAMakeReaction caches the scheduler's
  // material in its constructor, is built later, when the chemistry first
  // runs, so the material installed here is the one it sees.
  G4DNAChemistryManager::Instance()->Initialize();
  if (G4Scheduler::Instance()->GetScavengerMaterial() == nullptr)
  {
    auto material = std::make_unique<G4DNAScavengerMaterial>(fWorld.get());
    material->SetCounterAgainstTime();
    G4Scheduler::Instance()->SetScavengerMaterial(std::move(material));
  }
}

void PureWaterChemistryList::RegisterBulkReactionProcesses()
{
  auto* helper = G4PhysicsListHelper::GetPhysicsListHelper();
  // Registered on each configuration's own definition (OHm, Om, HO2m, O2m and
  // O3m have private definitions, see the main project's DnaChemistryList).
  for (const auto& entry : PureWaterReactions::BuildPureWaterBulkReactions())
  {
    const auto* molecule = Conf(entry.molecule);
    auto* process = new G4DNAScavengerProcess("G4DNAScavengerProcess", *fWorld->GetChemistryBoundary());
    for (const auto& reaction : entry.reactions)
      process->SetReaction(molecule, MakeBulkReactionData(molecule, reaction));
    helper->RegisterProcess(process, const_cast<G4MoleculeDefinition*>(molecule->GetDefinition()));
  }
}

}  // namespace drs
