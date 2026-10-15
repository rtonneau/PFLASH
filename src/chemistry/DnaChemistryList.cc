/// \file DnaChemistryList.cc
/// \brief Implementation of the DnaChemistryList class
///
/// Structure mirrors G4EmDNAChemistry_option3 and the UHDR example's
/// EmDNAChemistry. Molecules + water dissociation channels come from
/// G4ChemDissociationChannels_option1. The reaction content (ordinary
/// reaction table + bulk-reaction list) comes from the Chemistry selected with
/// /chem/select before /run/initialize (default PureWater, the portable
/// PureWaterReactions.cc: pure-water radiolysis plus the O2-derived
/// second-order network). See docs/adr/0002-named-chemistries.md.
///
/// The pH-driven acid-base buffer equilibria against the bulk H3Op(B) /
/// OHm(B) pseudo-species (UHDR: ChemPureWaterBuilder::WaterScavengerReaction)
/// are registered as per-molecule G4DNAScavengerProcess from the
/// bulk-reaction data list (PureWaterReactions::BuildPureWaterBulkReactions).
/// The PureWater chemistry always carries the full network -- baseline
/// aqueous chemistry (it can produce O2 from pure water radiolysis on its
/// own), not gated by any scavenger. See
/// docs/adr/0001-baseline-acid-base-buffer.md. The same list holds the
/// scavenger reactions against exogenous bulk species set with
/// /chem/env/scavenger (docs/adr/0004-scavenger-reactions-per-chemistry.md).
///
/// Time-step model (/process/chem/TimeStepModel): IRT_syn (default, may hand
/// over to the mesoscopic stage, /chem/meso/enable) or SBS (step-by-step,
/// particle-based stage only, no mesoscopic stage). IRT is not supported.

#include "chemistry/DnaChemistryList.hh"

#include "chemistry/BuiltInChemistries.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/ChemistryRegistry.hh"
#include "chemistry/ChemistrySelectMessenger.hh"
#include "chemistry/MesoMessenger.hh"
#include "chemistry/MesoSettings.hh"
#include "geometry/DetectorConstruction.hh"
#include "geometry/DnaChemistryWorld.hh"
#include "geometry/ScavengerSpec.hh"
#include "chemistry/ChemistryTypes.hh"
#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "scoring/ReactionCounter.hh"
#include "chemistry/ReactionTableDump.hh"
#include "chemistry/ScavengerReactionAccess.hh"

#include "G4ApplicationState.hh"
#include "G4ChemDissociationChannels_option1.hh"
#include "G4DNABoundingBox.hh"
#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4GenericMessenger.hh"
#include "G4Scheduler.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeDefinition.hh"
#include "G4MoleculeTable.hh"
#include "G4PhysicsListHelper.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessTable.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

// Chemical-stage processes
#include "G4DNABrownianTransportation.hh"
#include "G4DNAElectronHoleRecombination.hh"
#include "G4DNAElectronSolvation.hh"
#include "G4DNAMolecularDissociation.hh"
#include "G4DNASancheExcitationModel.hh"
#include "G4DNAScavengerProcess.hh"
#include "G4DNAVibExcitation.hh"
#include "G4DNAWaterDissociationDisplacer.hh"

// Time-step model
#include "G4ChemTimeStepModel.hh"
#include "G4DNAIndependentReactionTimeModel.hh"
#include "G4DNAMolecularStepByStepModel.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4EmParameters.hh"

// Particles
#include "G4Electron.hh"
#include "G4H2O.hh"

#include "G4PhysicsConstructorFactory.hh"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

G4_DECLARE_PHYSCONSTR_FACTORY(DnaChemistryList);

namespace
{
using MolConf = const G4MolecularConfiguration*;

MolConf Conf(const G4String& name, const G4String& caller)
{
  auto* p = G4MoleculeTable::Instance()->GetConfiguration(name);
  if (p == nullptr) {
    G4Exception(("DnaChemistryList::" + caller).c_str(), "MissingSpecies", FatalException,
                (G4String("Unknown species configuration: ") + name).c_str());
  }
  return p;
}

/// One bulk reaction of `molecule` as reaction data: the configurations,
/// products and reaction type, resolved once for both consumers (the
/// G4DNAScavengerProcess and the reaction table each own their copy).
G4DNAMolecularReactionData* MakeBulkReactionData(MolConf molecule,
                                                 const ChemistryTypes::BulkReaction& reaction,
                                                 const G4String& caller)
{
  auto* rd = new G4DNAMolecularReactionData(reaction.rate, molecule, Conf(reaction.partner, caller));
  for (const auto& product : reaction.products) {
    rd->AddProduct(Conf(product, caller));
  }
  // Type 1 (partially diffusion-controlled) is skipped under SBS; the acid-base
  // types 6/7/8 are not diffusion classes and always apply.
  if (reaction.reactionType != 0 &&
      (reaction.reactionType != 1 || ChemistryTypes::PartialReactionsEnabled())) {
    rd->SetReactionType(reaction.reactionType);
  }
  return rd;
}

/// Product names of `rd`, sorted, for an order-independent comparison.
std::vector<G4String> SortedProducts(const G4DNAMolecularReactionData& rd)
{
  std::vector<G4String> names;
  for (G4int i = 0; i < rd.GetNbProducts(); ++i) {
    names.push_back(rd.GetProduct(i)->GetName());
  }
  std::sort(names.begin(), names.end());
  return names;
}
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryList::DnaChemistryList()
  : G4VUserChemistryList(true)
{
  // Register with the chemistry manager (also activates the chemical stage).
  // The (true) flag marks this object as a G4VPhysicsConstructor so the manager
  // releases - rather than deletes - it, leaving ownership with the PhysicsList
  // that holds it (avoids a double free).
  G4DNAChemistryManager::Instance()->SetChemistryList(this);

  // Chemistries are registered before any macro can /chem/select one. The
  // manager singleton above already exists, so /chem/ is there for the commands.
  BuiltInChemistries::Register();
  fSelectMessenger = std::make_unique<ChemistrySelectMessenger>();
  fMesoMessenger = std::make_unique<MesoMessenger>();

  fMessenger = std::make_unique<G4GenericMessenger>(this, "/chem/reaction/",
                                                    "Chemistry reaction-table diagnostics");
  auto& dumpCmd = fMessenger->DeclareProperty(
    "dump", fReactionDumpFile,
    "Write the full reaction table (bimolecular + bulk networks) to <filename>.");
  dumpCmd.SetStates(G4State_PreInit);

  auto& timeBinsFixedCmd = fMessenger->DeclareMethodWithUnit(
    "timeBinsFixed", "picosecond", &DnaChemistryList::SetReactionTimeBinsFixed,
    "Bin the reaction-count output (Reactions.Txt / Reactions_nt_reactions.csv) at this fixed "
    "time step, up to the chemistry scheduler's end time. Mutually exclusive with timeBinsList "
    "(issuing both is a fatal configuration error). Neither issued -> a built-in 7-edge default "
    "table is used.");
  timeBinsFixedCmd.SetStates(G4State_PreInit);

  // DeclareProperty, not DeclareMethod: see fReactionTimeBinsList's doc comment.
  auto& timeBinsListCmd = fMessenger->DeclareProperty(
    "timeBinsList", fReactionTimeBinsList,
    "Bin the reaction-count output at these explicit edges: '<e1> <e2> ... <eN> <unit>' (e.g. "
    "'1 10 100 1000 picosecond'). Mutually exclusive with timeBinsFixed (issuing both is a fatal "
    "configuration error).");
  timeBinsListCmd.SetStates(G4State_PreInit);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DnaChemistryList::~DnaChemistryList() = default;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const ChemistryRegistry::Chemistry* DnaChemistryList::SelectedChemistry(
  const G4String& caller) const
{
  const auto* chemistry = ChemistryRegistry::Selected();
  if (chemistry == nullptr) {
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "NoChemistry", FatalException,
                "No chemistry is selected and the default is not registered.");
  }
  return chemistry;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::SetReactionTimeBinsFixed(G4double width)
{
  fReactionBinWidthSet = true;
  fReactionBinWidth = width;
}

void DnaChemistryList::ApplyReactionTimeBinning() const
{
  const G4bool listRequested = !fReactionTimeBinsList.empty();
  if (fReactionBinWidthSet && listRequested) {
    G4Exception("DnaChemistryList::ApplyReactionTimeBinning", "ConflictingTimeBins", FatalException,
                "/chem/reaction/timeBinsFixed and /chem/reaction/timeBinsList were both issued -- "
                "use only one per run.");
    return;
  }

  if (fReactionBinWidthSet) {
    const G4double endTime = G4Scheduler::Instance()->GetEndTime();
    std::vector<G4double> edges;
    for (G4double edge = fReactionBinWidth; edge < endTime + fReactionBinWidth;
         edge += fReactionBinWidth) {
      edges.push_back(edge);
    }
    ReactionCounter::ConfigureBinEdges(edges);
    return;
  }

  if (listRequested) {
    std::vector<G4double> edges;
    G4String error;
    if (!ReactionCounter::ParseBinEdgesList(fReactionTimeBinsList, edges, error)) {
      G4Exception("DnaChemistryList::ApplyReactionTimeBinning", "InvalidTimeBinsList", FatalException,
                  error.c_str());
      return;
    }
    ReactionCounter::ConfigureBinEdges(edges);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

const DnaChemistryWorld* DnaChemistryList::ChemistryWorld(const G4String& caller) const
{
  const auto* detector = dynamic_cast<const DetectorConstruction*>(
    G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  const auto* world =
    (detector != nullptr) ? dynamic_cast<const DnaChemistryWorld*>(detector->GetChemistryWorld())
                          : nullptr;
  if (world == nullptr) {
    G4Exception((G4String("DnaChemistryList::") + caller).c_str(), "NoChemistryWorld",
                FatalException, "Expected a DetectorConstruction owning a DnaChemistryWorld.");
  }
  return world;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructMolecule()
{
  // Standard radiolysis set + "(B)" bulk pseudo-species + water dissociation.
  G4ChemDissociationChannels_option1::ConstructMolecule();

  // Required by G4DNAScavengerProcess (member init: GetConfiguration("H2O")).
  G4MoleculeTable::Instance()->CreateConfiguration("H2O", G4H2O::Definition());

  // Molecules only the selected Chemistry needs (ADR 0007). This runs when the
  // physics list is handed to the run manager, before any macro, so it only
  // sees a Chemistry selected in code; /chem/select calls the hook itself
  // (ChemistrySelectMessenger).
  const auto* chemistry = SelectedChemistry("ConstructMolecule");
  if (chemistry->constructMolecules != nullptr) {
    chemistry->constructMolecules();
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructDissociationChannels()
{
  G4ChemDissociationChannels_option1::ConstructDissociationChannels();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable)
{
  // Populate the chemistry-world bulk composition here (master-only phase ->
  // race-free), before it is read by G4DNAScavengerMaterial.
  auto* chemWorld =
    const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructReactionTable"));
  chemWorld->ConstructChemistryComponents();

  // A scavenger whose species no bulk reaction of the selected Chemistry uses
  // would sit in the bulk without reacting. Master-only, so it warns once.
  const auto* chemistry = SelectedChemistry("ConstructReactionTable");
  for (const auto& species :
       ScavengerSpec::InertSpecies(chemWorld->GetScavengers(), chemistry->buildBulkReactions())) {
    DnaLogger::Print(DnaLogger::Level::Warning,
                     "[DnaChemistryList] scavenger " + species +
                       " is inert: no bulk reaction of chemistry " + chemistry->name +
                       " uses it as a partner");
  }

  // Ordinary (non-bulk) reactions of the selected Chemistry (default PureWater).
  chemistry->buildReactions(reactionTable);

  // Its bulk reactions too, for the mesoscopic stage (UHDR:
  // ChemPureWaterBuilder::WaterScavengerReaction).
  AddBulkReactionsToTable(reactionTable, chemistry->buildBulkReactions());
  WarnOnNegativeActivationRates(reactionTable);

  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryList] chemistry = " + chemistry->name +
                     ", reaction table constructed");
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::CheckTimeStepModel() const
{
  const G4ChemTimeStepModel model = G4EmParameters::Instance()->GetTimeStepModel();
  if (model != G4ChemTimeStepModel::IRT_syn && model != G4ChemTimeStepModel::SBS) {
    G4Exception("DnaChemistryList::CheckTimeStepModel", "UnsupportedTimeStepModel",
                FatalException,
                (G4String("/process/chem/TimeStepModel ") + ChemUtils::ToString(model) +
                 ": only IRT_syn (default) and SBS are supported (IRT is not).")
                  .c_str());
    return;
  }
  const G4bool sbs = (model == G4ChemTimeStepModel::SBS);
  // SBS keeps every reaction fully diffusion-controlled (type 0, as in chem1-chem6
  // and the SBS reference); IRT_syn applies the catalogue's type 1. Read by the
  // Chemistry builders and MakeBulkReactionData, which run after this
  // (RegisterBulkReactionProcesses, then Initialize -> ConstructReactionTable).
  ChemistryTypes::SetPartialReactionsEnabled(!sbs);
  const auto& meso = MesoSettings::Current();
  if (MesoSettings::ExplicitlyEnabledWithSbs(meso, sbs)) {
    G4Exception("DnaChemistryList::CheckTimeStepModel", "MesoWithSbs", FatalException,
                "/chem/meso/enable true was given with /process/chem/TimeStepModel SBS: the "
                "SBS model has no mesoscopic stage. Drop the command or set it to false.");
    return;
  }
  // Default chemistry end time: 1 s with the mesoscopic stage, 1 us without.
  // ActionInitialization::Build() does the same, but in Serial mode it runs
  // before the macro, so this is the value that counts there; a
  // /scheduler/endTime after /run/initialize overrides it.
  G4Scheduler::Instance()->SetEndTime(MesoSettings::StageEnabled(meso, sbs) ? 1. * s : 1. * us);
  // G4cout, not DnaLogger: the run configuration is printed at any logger
  // level (DnaLogger is Quiet by default).
  G4cout << "[DnaChemistryList] time-step model = " << ChemUtils::ToString(model)
         << ", mesoscopic stage " << (MesoSettings::StageEnabled(meso, sbs) ? "on" : "off")
         << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructTimeStepModel(G4DNAMolecularReactionTable* /*reactionTable*/)
{
  // Validated earlier by CheckTimeStepModel (ConstructProcess, master); this
  // runs per thread, at the first event in Serial mode.
  switch (G4EmParameters::Instance()->GetTimeStepModel()) {
    case G4ChemTimeStepModel::IRT_syn:
      // G4EmDNAChemistry_option3 / UHDR EmDNAChemistry IRT_syn branch; may
      // hand over to the mesoscopic stage (TimeStepAction).
      RegisterTimeStepModel(new G4DNAIndependentReactionTimeModel(), 0);
      break;
    case G4ChemTimeStepModel::SBS:
      // Step-by-step Brownian dynamics (chem1-chem6); particle-based stage
      // only, up to the end time.
      RegisterTimeStepModel(new G4DNAMolecularStepByStepModel(), 0);
      break;
    default:
      G4Exception("DnaChemistryList::ConstructTimeStepModel", "UnsupportedTimeStepModel",
                  FatalException,
                  "/process/chem/TimeStepModel: only IRT_syn (default) and SBS are supported.");
      return;
  }

  DnaLogger::Print(DnaLogger::Level::Info,
                   G4String("[DnaChemistryList] time-step model registered: ") +
                     ChemUtils::GetCurrentTimeStepModelName());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::ConstructProcess()
{
  // At /run/initialize, once all PreInit commands are in: an unsupported
  // model or /chem/meso/enable true with SBS stops the run here.
  if (!G4Threading::IsWorkerThread()) {
    CheckTimeStepModel();
  }

  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();

  // Extend the Sanche vibrational-excitation model down to thermal energies.
  auto* vibProcess =
    G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAVibExcitation", "e-");
  if (vibProcess != nullptr) {
    auto* vib = dynamic_cast<G4DNAVibExcitation*>(vibProcess);
    if (vib != nullptr) {
      auto* sanche = dynamic_cast<G4DNASancheExcitationModel*>(vib->EmModel());
      if (sanche != nullptr) {
        sanche->ExtendLowEnergyLimit(0.025 * eV);
      }
    }
  }

  // Electron solvation: free e- -> e_aq (register once if not already present).
  if (G4ProcessTable::GetProcessTable()->FindProcess("e-_G4DNAElectronSolvation", "e-") ==
      nullptr) {
    ph->RegisterProcess(new G4DNAElectronSolvation("e-_G4DNAElectronSolvation"),
                        G4Electron::Definition());
  }

  auto* moleculeTable = G4MoleculeTable::Instance();
  auto iterator = moleculeTable->GetDefintionIterator();
  iterator.reset();
  while (iterator()) {
    auto* moleculeDef = iterator.value();

    if (moleculeDef != G4H2O::Definition()) {
      // Both models (IRT_syn, SBS) diffuse every molecule with Brownian
      // transportation.
      ph->RegisterProcess(new G4DNABrownianTransportation(), moleculeDef);
    }
    else {
      moleculeDef->GetProcessManager()->AddRestProcess(new G4DNAElectronHoleRecombination(), 2);
      auto* dissociation = new G4DNAMolecularDissociation("H2O_DNAMolecularDecay");
      dissociation->SetDisplacer(moleculeDef, new G4DNAWaterDissociationDisplacer);
      moleculeDef->GetProcessManager()->AddRestProcess(dissociation, 1);
    }
  }

  auto* chemWorld = const_cast<DnaChemistryWorld*>(ChemistryWorld("ConstructProcess"));
  RegisterBulkReactionProcesses(*chemWorld->GetChemistryBoundary(),
                                SelectedChemistry("ConstructProcess")->buildBulkReactions());

  // Triggers InitializeMaster() -> ConstructReactionTable() ->
  // DnaChemistryWorld::ConstructChemistryComponents(): the bulk composition is
  // populated after this call returns.
  G4DNAChemistryManager::Instance()->Initialize();

  // Install the bulk-scavenger material now that the composition is known, and
  // before G4DNAScavengerProcess::BuildPhysicsTable() queries the scheduler.
  // Runs once (serial) or per worker thread (MT); the scheduler is thread-local.
  // Unconditional: the bulk-reaction scavenger processes registered above
  // are always active, so this material must always be installed -- without
  // it, G4DNAScavengerProcess::PostStepGetPhysicalInteractionLength would
  // dereference a null fpScavengerMaterial on the first step of any species
  // with a registered reaction.
  if (G4Scheduler::Instance()->GetScavengerMaterial() == nullptr) {
    auto scavenger = std::make_unique<G4DNAScavengerMaterial>(chemWorld);
    scavenger->SetCounterAgainstTime();
    G4Scheduler::Instance()->SetScavengerMaterial(std::move(scavenger));
  }

  // The initial mesh pixel count is capped (ADR 0006): say so once, from the
  // master, with the cell size the cap leaves (only when there is a mesh).
  const auto& meso = MesoSettings::Current();
  const G4bool sbs = G4EmParameters::Instance()->GetTimeStepModel() == G4ChemTimeStepModel::SBS;
  if (!G4Threading::IsWorkerThread() && MesoSettings::StageEnabled(meso, sbs)) {
    const G4double side = 2. * chemWorld->GetHalfBox();
    if (MesoSettings::PixelCountCapped(side, meso.voxelSize * mm)) {
      const G4double used = side / MesoSettings::PixelCount(side, meso.voxelSize * mm);
      DnaLogger::Print(DnaLogger::Level::Warning,
                       "[meso] pixel count capped at " + std::to_string(MesoSettings::kMaxPixels) +
                         " per side (G4DNAMesh index overflow, ADR 0006): requested cell " +
                         std::to_string(meso.voxelSize * mm / nm) + " nm, cell used " +
                         std::to_string(used / nm) + " nm");
    }
  }

  // Both networks (bimolecular + bulk) are fully constructed by this
  // point; opt-in dump for external checks (see /chem/reaction/dump).
  // Dump is written once, from the master (MT) or the only thread (Serial).
  if (!fReactionDumpFile.empty() && !G4Threading::IsWorkerThread()) {
    ReactionTableDump::DumpReactionTable(OutputDir::Resolve(fReactionDumpFile));
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::RegisterBulkReactionProcesses(
  const G4DNABoundingBox& boundary, const ChemistryTypes::BulkReactionList& list) const
{
  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();
  const G4String caller = "RegisterBulkReactionProcesses";

  // Registers against mol->GetDefinition() rather than a caller-supplied
  // class pointer: several species here (OHm, H3Op) have no dedicated
  // factory class, and using GetDefinition() uniformly avoids the previous
  // mismatch where Om's process was attached to G4Oxygen::Definition() (the
  // separate "Oxy" O(3P) species) instead of Om's actual definition (a
  // private G4MoleculeDefinition("O", ...) created inside
  // G4ChemDissociationChannels_option1::ConstructMolecule()) -- silently
  // making those reactions unreachable.
  for (const auto& entry : list) {
    auto* mol = Conf(entry.molecule, caller);
    auto* process = new ScavengerReactionAccess("G4DNAScavengerProcess", boundary);
    for (const auto& r : entry.reactions) {
      process->SetReaction(mol, MakeBulkReactionData(mol, r, caller));
    }
    auto* def = const_cast<G4MoleculeDefinition*>(mol->GetDefinition());
    ph->RegisterProcess(process, def);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::AddBulkReactionsToTable(G4DNAMolecularReactionTable* reactionTable,
                                               const ChemistryTypes::BulkReactionList& list) const
{
  const G4String caller = "AddBulkReactionsToTable";
  G4int added = 0;
  G4int shared = 0;
  for (const auto& entry : list) {
    auto* mol = Conf(entry.molecule, caller);
    for (const auto& r : entry.reactions) {
      auto* partner = Conf(r.partner, caller);
      // GetReactionData is fatal on an empty table, so ask only when it isn't.
      const auto* existing = reactionTable->GetNReactions() == 0
                               ? nullptr
                               : reactionTable->GetReactionData(mol, partner);
      if (existing == nullptr) {
        reactionTable->SetReaction(MakeBulkReactionData(mol, r, caller));
        ++added;
        continue;
      }

      // The pair is already there: a bulk partner that is also a tracked
      // species (O2) has one configuration for both, so the tracked-pair
      // entry serves the bulk reaction too. A second SetReaction would list
      // the pair twice, and the mesoscopic Gillespie sums every listed entry.
      const std::unique_ptr<G4DNAMolecularReactionData> candidate(
        MakeBulkReactionData(mol, r, caller));
      const G4double rate = existing->GetObservedReactionRateConstant();
      const G4bool sameRate =
        std::abs(rate - r.rate) <= 1e-9 * std::max(std::abs(rate), std::abs(r.rate));
      if (!sameRate || SortedProducts(*existing) != SortedProducts(*candidate)) {
        G4Exception(("DnaChemistryList::" + caller).c_str(), "ConflictingBulkReaction",
                    FatalException,
                    ("Bulk reaction " + mol->GetName() + " + " + partner->GetName() +
                     " differs (rate or products) from the reaction-table entry of the same "
                     "pair; a Chemistry must define both identically.")
                      .c_str());
        return;
      }
      ++shared;
      DnaLogger::Print(DnaLogger::Level::Debug,
                       "[DnaChemistryList] bulk reaction " + mol->GetName() + " + " +
                         partner->GetName() + " shares the existing reaction-table entry");
    }
  }
  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryList] bulk reactions in the reaction table: " +
                     std::to_string(added) + " added, " + std::to_string(shared) +
                     " sharing a tracked-pair entry");
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DnaChemistryList::WarnOnNegativeActivationRates(
  G4DNAMolecularReactionTable* reactionTable) const
{
  // SetReactionType(1) derives k_act = k_diff * k_obs / (k_diff - k_obs) from
  // the vdW radius (G4DNAMolecularReactionData::SetReactionType); k_obs at or
  // above that k_diff gives k_act <= 0, an ill-defined partial reaction.
  G4int nType0 = 0;
  G4int nType1 = 0;
  G4int nOther = 0;
  for (const auto* rd : reactionTable->GetVectorOfReactionData()) {
    const G4int type = rd->GetReactionType();
    (type == 0 ? nType0 : type == 1 ? nType1 : nOther)++;
    if (type == 1 && rd->GetActivationRateConstant() <= 0.) {
      DnaLogger::Print(DnaLogger::Level::Warning,
                       "[DnaChemistryList] partially diffusion-controlled reaction " +
                         rd->GetReactant1()->GetName() + " + " + rd->GetReactant2()->GetName() +
                         " has k_obs >= k_diff (vdW radius): activation rate <= 0");
    }
  }
  // Type 1 only with IRT_syn (ChemistryTypes::PartialReactionsEnabled); SBS has none.
  DnaLogger::Print(DnaLogger::Level::Info,
                   "[DnaChemistryList] reaction types: " + std::to_string(nType0) +
                     " fully diffusion-controlled (0), " + std::to_string(nType1) +
                     " partially diffusion-controlled (1), " + std::to_string(nOther) +
                     " other");
}
