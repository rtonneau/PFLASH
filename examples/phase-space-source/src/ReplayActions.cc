#include "ReplayActions.hh"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

#include "PhaseSpaceSource.hh"
#include "ReplayChemistryList.hh"
#include "ReplayDetectorConstruction.hh"

#include "G4DNAChemistryManager.hh"
#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4H2O.hh"
#include "G4LogicalVolume.hh"
#include "G4MolecularConfiguration.hh"
#include "G4MoleculeCounter.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeCounterTimeComparer.hh"
#include "G4Run.hh"
#include "G4Scheduler.hh"
#include "G4StackManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"

namespace phsp
{

namespace
{

G4bool InBox(const G4StepPoint* point, const G4LogicalVolume* box)
{
  const G4VPhysicalVolume* volume = point->GetPhysicalVolume();
  return volume != nullptr && volume->GetLogicalVolume() == box;
}

G4bool SameCounts(const PreChemicalCounts& a, const PreChemicalCounts& b)
{
  return a.ionisedWater == b.ionisedWater && a.excitedWater == b.excitedWater && a.attachedWater == b.attachedWater &&
         a.solvatedElectrons == b.solvatedElectrons && a.outsideBox == b.outsideBox;
}

// The time-step action of this worker thread (G4Scheduler does not own it).
G4ThreadLocal ReplayTimeStepAction* gTimeStepAction = nullptr;

// One G4MoleculeCounter on this worker, as in chem4-chem6: reset before each
// event (the counters then also check that times only increase), not
// accumulated into the master, H2O ignored, 1 ps time precision.
G4int BuildMoleculeCounter()
{
  auto* manager = G4MoleculeCounterManager::Instance();
  manager->SetResetCountersBeforeEvent(true);
  manager->SetAccumulateCounterIntoMaster(false);
  auto counter = std::make_unique<G4MoleculeCounter>("ReplaySpecies");
  counter->SetTimeComparer(G4MoleculeCounterTimeComparer::CreateWithFixedPrecision(1 * picosecond));
  counter->IgnoreMolecule(G4H2O::Definition());
  return manager->RegisterCounter(std::move(counter));
}

}  // namespace

// ---- Results ----------------------------------------------------------------------

void ReplayResults::Add(ReplayHistoryRow row)
{
  std::lock_guard<std::mutex> lock(fMutex);
  fRows.push_back(std::move(row));
}

std::vector<ReplayHistoryRow> ReplayResults::SortedRows() const
{
  std::vector<ReplayHistoryRow> rows;
  {
    std::lock_guard<std::mutex> lock(fMutex);
    rows = fRows;
  }
  std::sort(rows.begin(), rows.end(),
            [](const ReplayHistoryRow& a, const ReplayHistoryRow& b) { return a.globalIndex < b.globalIndex; });
  return rows;
}

// ---- Pre-chemical counter ----------------------------------------------------------

void PreChemicalCounter::CountPosition(const G4Track* track)
{
  const G4ThreeVector& position = track->GetPosition();
  const G4double limit = fBoxHalfSide * (1 + 1e-9);
  if (std::abs(position.x()) > limit || std::abs(position.y()) > limit || std::abs(position.z()) > limit)
    ++fCounts.outsideBox;
}

void PreChemicalCounter::CreateWaterMolecule(G4int modification, G4int, G4double, const G4Track* track)
{
  switch (modification)
  {
    case eIonizedMolecule: ++fCounts.ionisedWater; break;
    case eExcitedMolecule: ++fCounts.excitedWater; break;
    case eDissociativeAttachment: ++fCounts.attachedWater; break;
    default:
      G4Exception("PreChemicalCounter::CreateWaterMolecule", "PhspReplay031", FatalException,
                  "unknown electronic modification of a water molecule");
      return;
  }
  CountPosition(track);
}

void PreChemicalCounter::CreateSolvatedElectron(const G4Track* track, G4ThreeVector*)
{
  ++fCounts.solvatedElectrons;
  CountPosition(track);
}

// ---- Event ---------------------------------------------------------------------------

void ReplayEventAction::BeginOfEventAction(const G4Event* event)
{
  fDeposit = 0;
  fTracksOutside = 0;
  if (!fChemistry) return;
  ResetMoleculesKilledOutsideWater();
  auto* chemistry = G4DNAChemistryManager::Instance();
  chemistry->BeginOfEventAction(event);  // resets the molecule counter (SetResetCountersBeforeEvent)
  auto counter = std::make_unique<PreChemicalCounter>(fDetector.BoxHalfSide());
  fPreChemical = counter.get();
  chemistry->SetPhysChemIO(std::move(counter));
  fHaveSnapshot = false;
  fSnapshot = {};
}

void ReplayEventAction::TakePreChemicalSnapshot()
{
  if (fPreChemical == nullptr || fHaveSnapshot) return;
  fSnapshot = fPreChemical->Counts();
  fHaveSnapshot = true;
}

void ReplayEventAction::FillSpecies(ReplayHistoryRow& row) const
{
  const auto* counter = G4MoleculeCounterManager::Instance()->GetMoleculeCounter<G4MoleculeCounter>(fMoleculeCounterId);
  if (counter == nullptr)
  {
    G4Exception("ReplayEventAction::FillSpecies", "PhspReplay032", FatalException,
                "the molecule counter of this thread is missing");
    return;
  }
  const G4double endTime = G4Scheduler::Instance()->GetEndTime();
  const std::vector<G4double> grid = SpeciesTimeGrid(endTime);
  row.chemistryEndTimeNs = endTime / ns;
  row.speciesTimesNs.clear();
  for (const G4double time : grid) row.speciesTimesNs.push_back(time / ns);
  for (const auto& index : counter->GetMapIndices())
  {
    SpeciesSeries series;
    series.name = index.Molecule->GetName();
    series.counts.reserve(grid.size());
    for (const G4double time : grid) series.counts.push_back(counter->GetNbMoleculesAtTime(index, time));
    row.species.push_back(std::move(series));
  }
  std::sort(row.species.begin(), row.species.end(),
            [](const SpeciesSeries& a, const SpeciesSeries& b) { return a.name < b.name; });
}

void ReplayEventAction::EndOfEventAction(const G4Event* event)
{
  const auto* information = dynamic_cast<const ReplayEventInformation*>(event->GetUserInformation());
  if (information == nullptr)
  {
    G4Exception("ReplayEventAction::EndOfEventAction", "PhspReplay030", FatalException,
                "event without ReplayEventInformation: was it generated by PhaseSpaceSource?");
    return;
  }
  ReplayHistoryRow row;
  row.globalIndex = information->globalIndex;
  row.fileId = information->key.fileId;
  row.historyIndex = information->key.historyIndex;
  row.stage1EventId = information->entry.stage1EventId;
  row.boxIx = information->entry.boxIx;
  row.boxIy = information->entry.boxIy;
  row.recordCount = information->entry.recordCount;
  row.stage1DepositEv = information->entry.inBoxDepositEv;
  row.stage2DepositEv = fDeposit / eV;
  row.tracksKilledOutsideBox = fTracksOutside;
  if (fChemistry)
  {
    const PreChemicalCounts finalCounts = fPreChemical != nullptr ? fPreChemical->Counts() : PreChemicalCounts{};
    // The snapshot is taken when the chemistry starts; the DNA models create
    // nothing after that, so it equals the final counts.
    if (fHaveSnapshot && !SameCounts(fSnapshot, finalCounts))
      G4Exception("ReplayEventAction::EndOfEventAction", "PhspReplay033", FatalException,
                  "pre-chemical species were created after the chemistry started");
    row.preChemical = fHaveSnapshot ? fSnapshot : finalCounts;
    FillSpecies(row);
    row.moleculesKilledOutsideWater = MoleculesKilledOutsideWater();
    auto* chemistry = G4DNAChemistryManager::Instance();
    chemistry->EndOfEventAction(event);
    chemistry->SetPhysChemIO(nullptr);  // deletes the counter
    fPreChemical = nullptr;
  }
  fResults.Add(std::move(row));
}

// ---- Stepping: deposit in the box, kill on leaving -------------------------------------

void ReplaySteppingAction::UserSteppingAction(const G4Step* step)
{
  G4Track* track = step->GetTrack();
  const G4LogicalVolume* box = fDetector.BoxLogical();
  if (!InBox(step->GetPreStepPoint(), box))
  {
    fEventAction.CountTrackOutsideBox();
    track->SetTrackStatus(fStopAndKill);
    return;
  }
  fEventAction.AddDeposit(step->GetTotalEnergyDeposit());
  if (!InBox(step->GetPostStepPoint(), box)) track->SetTrackStatus(fStopAndKill);
}

// ---- Tracking: no physics track starts outside the box ----------------------------------

void ReplayTrackingAction::PreUserTrackingAction(const G4Track* track)
{
  // The track is located (G4SteppingManager::SetInitialStep) before this is called.
  const G4VPhysicalVolume* volume = track->GetVolume();
  if (volume != nullptr && volume->GetLogicalVolume() == fEventAction.Detector().BoxLogical()) return;
  fEventAction.CountTrackOutsideBox();
  const_cast<G4Track*>(track)->SetTrackStatus(fStopAndKill);
}

// ---- Stacking: start the chemistry -------------------------------------------------------

void ReplayStackingAction::NewStage()
{
  if (stackManager->GetNTotalTrack() == 0) G4DNAChemistryManager::Instance()->Run();
}

// ---- Run -----------------------------------------------------------------------------------

void ReplayRunAction::BeginOfRunAction(const G4Run* run)
{
  if (fSource == nullptr) return;  // worker
  if (fChemistry != nullptr && fChemistry->enabled != G4DNAChemistryManager::IsActivated())
  {
    G4Exception("ReplayRunAction::BeginOfRunAction", "PhspReplay034", FatalException,
                fChemistry->enabled ? "/phsp/chem/enable is true but the chemistry was not built: issue "
                                      "/run/initialize from the top-level macro, not from /control/execute"
                                    : "/phsp/chem/enable is false but the Geant4-DNA chemistry is active");
    return;
  }
  if (fChemistry != nullptr && fChemistry->enabled)
  {
    // Diffusion length of the fastest species against the water margin.
    const FastestSpecies fastest = FindFastestSpecies();
    const G4double endTime = G4Scheduler::Instance()->GetEndTime();
    const G4double length = std::sqrt(6. * fastest.diffusionCoefficient * endTime);
    if (length > fChemistry->chemMargin / 3.)
    {
      std::ostringstream message;
      message << "sqrt(6 D t_end) of the fastest species (" << fastest.name << ", D = "
              << fastest.diffusionCoefficient / (m2 / s) << " m2/s) at t_end = " << endTime / ns << " ns is "
              << length / nm << " nm, more than a third of the water margin (" << fChemistry->chemMargin / nm
              << " nm): molecules may leave the water and be killed; raise /phsp/geom/chemMargin";
      G4Exception("ReplayRunAction::BeginOfRunAction", "PhspReplay035", JustWarning, message.str().c_str());
    }
  }
  fStart = std::chrono::steady_clock::now();
  // G4MTRunManager::Initialize runs a BeamOn(0) to start the workers: no range.
  if (run->GetNumberOfEventToBeProcessed() > 0)
    fSource->BeginRun(static_cast<std::uint64_t>(run->GetNumberOfEventToBeProcessed()));
}

void ReplayRunAction::EndOfRunAction(const G4Run* run)
{
  if (fResults == nullptr || run->GetNumberOfEventToBeProcessed() == 0) return;
  fResults->AddWallTime(std::chrono::duration<double>(std::chrono::steady_clock::now() - fStart).count());
}

// ---- Action initialization ----------------------------------------------------------------

void ReplayActionInitialization::BuildForMaster() const
{
  SetUserAction(new ReplayRunAction(&fSource, &fResults, &fChemistry));
}

void ReplayActionInitialization::Build() const
{
  // Build runs on each worker after the master's /run/initialize, so the
  // PreInit chemistry settings are final here; the molecule counter is
  // registered before the worker initialises its chemistry
  // (G4MTRunManagerKernel::StartThread: Build, then Initialize).
  const G4bool chemistry = fChemistry.enabled;
  const G4int counterId = chemistry ? BuildMoleculeCounter() : -1;
  SetUserAction(new PhaseSpaceSource(fSource));
  SetUserAction(new ReplayRunAction(nullptr, nullptr, nullptr));
  auto* eventAction = new ReplayEventAction(fResults, fDetector, chemistry, counterId);
  SetUserAction(eventAction);
  SetUserAction(new ReplayTrackingAction(*eventAction));
  SetUserAction(new ReplaySteppingAction(fDetector, *eventAction));
  if (chemistry)
  {
    SetUserAction(new ReplayStackingAction());
    delete gTimeStepAction;
    gTimeStepAction = new ReplayTimeStepAction(*eventAction);
    G4Scheduler::Instance()->SetUserAction(gTimeStepAction);
  }
}

}  // namespace phsp
