#include "ReplayChemistryList.hh"

#include <atomic>
#include <cmath>
#include <exception>
#include <sstream>

#include "ChemistryTypes.hh"
#include "PureWaterReactions.hh"

#include "G4ChemDissociationChannels_option1.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNABrownianTransportation.hh"
#include "G4ParticleChangeForTransport.hh"
#include "G4Track.hh"
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
#include "G4Threading.hh"

namespace phsp
{

namespace
{

// -1 not known yet, 0 not applied, 1 applied (master's ConstructProcess).
std::atomic<int> gSancheExtended{-1};

G4ThreadLocal std::uint64_t gKilledOutsideWater = 0;

// Called by G4DNABrownianTransportation::Diffusion when a molecule's step
// starts in a material without water: kills it, as Geant4 does without an
// action, and counts it.
class OutsideWaterAction : public G4BrownianAction
{
 public:
  void Transport(const G4Track&, G4ParticleChangeForTransport& change) override
  {
    ++gKilledOutsideWater;
    change.ProposeEnergy(0.);
    change.ProposeTrackStatus(fStopAndKill);
  }
};

// One per thread, shared by the thread's Brownian transports (which do not
// delete it, G4DNABrownianTransportation.cc).
G4ThreadLocal OutsideWaterAction* gOutsideWaterAction = nullptr;

const G4MolecularConfiguration* Conf(const G4String& name)
{
  auto* configuration = G4MoleculeTable::Instance()->GetConfiguration(name, false);
  if (configuration == nullptr)
    G4Exception("ReplayChemistryList", "PhspChem001", FatalException,
                ("unknown species configuration: " + name).c_str());
  return configuration;
}

// Same conversion as PFLASH DnaChemistryList (MakeBulkReactionData).
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

// ---- Settings helpers -----------------------------------------------------------------

bool ParseO2Concentration(const std::string& text, double& molarity, std::string& error)
{
  std::istringstream in(text);
  std::string valueText, unit, extra;
  in >> valueText >> unit;
  if (valueText.empty() || unit.empty() || (in >> extra))
  {
    error = "expected '<value> <unit>' with unit M, mM, uM or %, got '" + text + "'";
    return false;
  }
  double value = 0;
  std::size_t parsed = 0;
  try
  {
    value = std::stod(valueText, &parsed);
  }
  catch (const std::exception&)
  {
    parsed = 0;
  }
  if (parsed == 0 || parsed != valueText.size() || !std::isfinite(value))
  {
    error = "invalid O2 concentration '" + valueText + "'";
    return false;
  }
  if (value < 0)
  {
    error = "negative O2 concentration " + valueText;
    return false;
  }
  double perUnit = 0;
  if (unit == "M")
    perUnit = 1;
  else if (unit == "mM")
    perUnit = 1e-3;
  else if (unit == "uM")
    perUnit = 1e-6;
  else if (unit == "%")
    perUnit = ReplayChemistrySettings::kO2SaturationMolarity / 100.;
  else
  {
    error = "unknown O2 unit '" + unit + "' (use M, mM, uM or %)";
    return false;
  }
  molarity = value * perUnit;
  return true;
}

std::vector<G4double> SpeciesTimeGrid(G4double endTime)
{
  std::vector<G4double> grid;
  const G4double start = 1. * picosecond;
  const G4double tolerance = 1e-9 * endTime;
  for (int k = 0; k <= 1000; ++k)
  {
    const G4double time = start * std::pow(10., static_cast<double>(k) / kSpeciesTimesPerDecade);
    if (time > endTime + tolerance) break;
    grid.push_back(time);
  }
  if (!grid.empty() && std::abs(grid.back() - endTime) <= tolerance)
    grid.back() = endTime;
  else
    grid.push_back(endTime);
  return grid;
}

std::uint64_t MoleculesKilledOutsideWater()
{
  return gKilledOutsideWater;
}

void ResetMoleculesKilledOutsideWater()
{
  gKilledOutsideWater = 0;
}

FastestSpecies FindFastestSpecies()
{
  FastestSpecies fastest;
  auto iterator = G4MoleculeTable::Instance()->GetConfigurationIterator();
  iterator.reset();
  while (iterator())
  {
    const G4MolecularConfiguration* configuration = iterator.value();
    const G4String& name = configuration->GetUserID();
    if (name.find("(B)") != std::string::npos) continue;
    if (configuration->GetDiffusionCoefficient() > fastest.diffusionCoefficient)
    {
      fastest.diffusionCoefficient = configuration->GetDiffusionCoefficient();
      fastest.name = configuration->GetName();
    }
  }
  return fastest;
}

// ---- Chemistry world ---------------------------------------------------------------------

ReplayChemistryWorld::ReplayChemistryWorld(G4double reservoirHalfSide, G4double pH, double o2Molarity)
  : fReservoirHalfSide(reservoirHalfSide), fPH(pH), fO2Molarity(o2Molarity)
{
  ConstructChemistryBoundary();
}

void ReplayChemistryWorld::ConstructChemistryBoundary()
{
  const G4double h = fReservoirHalfSide;
  // G4DNABoundingBox order: xhi xlo yhi ylo zhi zlo.
  fpChemistryBoundary = std::make_unique<G4DNABoundingBox>(G4DNABoundingBox{h, -h, h, -h, h, -h});
}

void ReplayChemistryWorld::ConstructChemistryComponents()
{
  // Constants of the Geant4 UHDR ChemistryWorld, as in PFLASH DnaChemistryWorld.
  const G4double pKw = 14.0;
  const G4double waterConcentration_M = 55.3;
  fpChemicalComponent.clear();
  fpChemicalComponent[Conf("H2O(B)")] = waterConcentration_M / (mole * liter);
  fpChemicalComponent[Conf("H3Op(B)")] = std::pow(10.0, -fPH) / (mole * liter);
  fpChemicalComponent[Conf("OHm(B)")] = std::pow(10.0, -(pKw - fPH)) / (mole * liter);
  // Dissolved O2 is the "O2" configuration, the partner of the three O2 bulk
  // reactions (PureWater's bulk list); 0 = absent, as in PFLASH.
  if (fO2Molarity > 0) fpChemicalComponent[Conf("O2")] = fO2Molarity / (mole * liter);
}

// ---- Chemistry list ------------------------------------------------------------------------

ReplayChemistryList::ReplayChemistryList(const ReplayChemistrySettings& settings)
  : G4VUserChemistryList(true),
    G4VPhysicsConstructor("ReplayChemistry"),
    fWorld(std::make_unique<ReplayChemistryWorld>(ReplayChemistrySettings::kBulkReservoirHalfSide, settings.pH,
                                                  settings.o2Molarity))
{
  if (!(settings.pH > 0 && settings.pH < 14))
    G4Exception("ReplayChemistryList", "PhspChem002", FatalErrorInArgument, "pH must be in (0, 14)");
  // The `true` above marks this object as a G4VPhysicsConstructor: the
  // physics list owns it and the chemistry manager only refers to it.
  G4DNAChemistryManager::Instance()->SetChemistryList(*this);
  // IRT_syn: the partially diffusion-controlled pairs are type 1. Set before
  // any builder runs.
  ChemistryTypes::SetPartialReactionsEnabled(true);
}

ReplayChemistryList::~ReplayChemistryList() = default;

G4bool ReplayChemistryList::SancheExtended()
{
  return gSancheExtended.load() == 1;
}

void ReplayChemistryList::ConstructMolecule()
{
  G4ChemDissociationChannels_option1::ConstructMolecule();
  // Required by G4DNAScavengerProcess (member init: GetConfiguration("H2O")).
  if (G4MoleculeTable::Instance()->GetConfiguration("H2O", false) == nullptr)
    G4MoleculeTable::Instance()->CreateConfiguration("H2O", G4H2O::Definition());
}

void ReplayChemistryList::ConstructDissociationChannels()
{
  G4ChemDissociationChannels_option1::ConstructDissociationChannels();
}

void ReplayChemistryList::ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable)
{
  // Master only (G4DNAChemistryManager::InitializeMaster), before any worker
  // builds its G4DNAScavengerMaterial from the composition.
  fWorld->ConstructChemistryComponents();
  // No mesoscopic stage: the bulk reactions act only through
  // G4DNAScavengerProcess, so they stay out of the reaction table.
  PureWaterReactions::BuildPureWaterReactions(reactionTable);
}

void ReplayChemistryList::ConstructTimeStepModel(G4DNAMolecularReactionTable* /*reactionTable*/)
{
  // IRT_syn only (ReplayPhysicsList::EnableChemistry refuses another model).
  RegisterTimeStepModel(new G4DNAIndependentReactionTimeModel(), 0);
}

void ReplayChemistryList::ConstructProcess()
{
  auto* helper = G4PhysicsListHelper::GetPhysicsListHelper();

  // Extend the Sanche vibrational-excitation model down to thermal energies
  // (G4EmDNAChemistry_option3); see SancheExtended().
  G4bool extended = false;
  if (auto* vib = dynamic_cast<G4DNAVibExcitation*>(
          G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAVibExcitation", "e-")))
    if (auto* sanche = dynamic_cast<G4DNASancheExcitationModel*>(vib->EmModel()))
    {
      sanche->ExtendLowEnergyLimit(0.025 * eV);
      extended = true;
    }
  if (!G4Threading::IsWorkerThread()) gSancheExtended.store(extended ? 1 : 0);

  if (G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAElectronSolvation", "e-") == nullptr)
    helper->RegisterProcess(new G4DNAElectronSolvation("e-_G4DNAElectronSolvation"), G4Electron::Definition());

  if (gOutsideWaterAction == nullptr) gOutsideWaterAction = new OutsideWaterAction();
  auto iterator = G4MoleculeTable::Instance()->GetDefintionIterator();
  iterator.reset();
  while (iterator())
  {
    G4MoleculeDefinition* molecule = iterator.value();
    if (molecule != G4H2O::Definition())
    {
      auto* brownian = new G4DNABrownianTransportation();
      brownian->SetBrownianAction(gOutsideWaterAction);
      helper->RegisterProcess(brownian, molecule);
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

  // The bulk material, one per thread (the G4Scheduler is thread-local), in
  // PFLASH DnaChemistryList's order. On a worker, Initialize() builds the
  // time-step model at once, and G4DNAMakeReaction caches the scheduler's
  // material in its constructor, so the material is installed first; the
  // master filled the composition at its own Initialize() (InitializeMaster
  // -> ConstructReactionTable), before any worker started.
  ReplayChemistryWorld* world = fWorld.get();
  const auto installBulkMaterial = [world]() {
    if (G4Scheduler::Instance()->GetScavengerMaterial() != nullptr) return;
    auto material = std::make_unique<G4DNAScavengerMaterial>(world);
    material->SetCounterAgainstTime();
    G4Scheduler::Instance()->SetScavengerMaterial(std::move(material));
  };
  if (G4Threading::IsWorkerThread()) installBulkMaterial();
  G4DNAChemistryManager::Instance()->Initialize();
  installBulkMaterial();

  // Default end time; a /scheduler/endTime after /run/initialize overrides it
  // (the master applies it, and every worker replays it before its next run).
  G4Scheduler::Instance()->SetEndTime(ReplayChemistrySettings::kDefaultEndTime);
}

void ReplayChemistryList::RegisterBulkReactionProcesses()
{
  auto* helper = G4PhysicsListHelper::GetPhysicsListHelper();
  // Registered on each configuration's own definition (OHm, Om, HO2m, O2m and
  // O3m have private definitions, see PFLASH DnaChemistryList).
  for (const auto& entry : PureWaterReactions::BuildPureWaterBulkReactions())
  {
    const auto* molecule = Conf(entry.molecule);
    auto* process = new G4DNAScavengerProcess("G4DNAScavengerProcess", *fWorld->GetChemistryBoundary());
    for (const auto& reaction : entry.reactions) process->SetReaction(molecule, MakeBulkReactionData(molecule, reaction));
    helper->RegisterProcess(process, const_cast<G4MoleculeDefinition*>(molecule->GetDefinition()));
  }
}

}  // namespace phsp
