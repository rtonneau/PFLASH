/// \file TimeStepAction.cc
/// \brief Implementation of the TimeStepAction class

#include "chemistry/TimeStepAction.hh"

#include "core/DnaLogger.hh"
#include "core/OutputDir.hh"
#include "chemistry/ChemUtils.hh"
#include "chemistry/MesoSettings.hh"
#include "chemistry/ReactionTableDump.hh"
#include "scoring/MesoSpatialFile.hh"

#include "G4DNABoundingBox.hh"
#include "G4DNAEventScheduler.hh"
#include "G4DNAMesh.hh"
#include "G4FakeMolecule.hh"
#include "G4H2O.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4DNAScavengerMaterial.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeTable.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Timer.hh"
#include "G4UserMeshAction.hh"
#include "G4VChemistryWorld.hh"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
/// Number of molecules held by a mesoscopic mesh (all cells, all species).
long long MeshTotal(const G4DNAMesh &mesh)
{
  long long total = 0;
  for (auto it = mesh.const_begin(); it != mesh.const_end(); ++it) {
    for (const auto &[molType, count] : std::get<2>(*it)) {
      total += static_cast<long long>(count);
    }
  }
  return total;
}

/// Pixels per side of a mesh, from its bounding box and cell size.
long long MeshPixels(const G4DNAMesh &mesh)
{
  return std::llround(2. * mesh.GetBoundingBox().halfSideLengthInY() / mesh.GetResolution());
}

G4String Format(G4double value)
{
  std::ostringstream os;
  os.precision(6);
  os << value;
  return os.str();
}

/// Molecule count of every species held by G4DNAScavengerMaterial (the bulk:
/// H2O(B), H3Op(B), OHm(B) and the /chem/env/scavenger species), in its
/// scavenger-table order. Its bulk reactions change these counts, so two
/// snapshots show what the mesoscopic stage consumed or produced in bulk.
std::vector<std::pair<const G4MolecularConfiguration *, long long>> BulkCounts()
{
  std::vector<std::pair<const G4MolecularConfiguration *, long long>> counts;
  auto *scavenger =
    dynamic_cast<G4DNAScavengerMaterial *>(G4Scheduler::Instance()->GetScavengerMaterial());
  if (scavenger == nullptr) return counts;
  for (const auto *conf : scavenger->GetScavengerList()) {
    counts.emplace_back(conf, static_cast<long long>(
                                scavenger->GetNumberMoleculePerVolumeUnitForMaterialConf(conf)));
  }
  return counts;
}

/// Species counts of a mesoscopic mesh (all cells), per molecular configuration.
using MeshCounts = std::map<const G4MolecularConfiguration *, long>;

MeshCounts MeshSpeciesCounts(const G4DNAMesh &mesh)
{
  MeshCounts counts;
  for (auto it = mesh.const_begin(); it != mesh.const_end(); ++it) {
    for (const auto &[molType, count] : std::get<2>(*it)) {
      counts[molType] += static_cast<long>(count);
    }
  }
  return counts;
}

/// Mesoscopic-stage mesh action, called by G4DNAEventScheduler around and
/// after every Gillespie step (reaction or diffusive jump).
///
/// 1. Records the species counts at the record times (always). This replaces
///    G4DNAEventScheduler's own counter map: in Geant4 11.4.1 its RecordTime()
///    runs only after a *reaction* step and records at most one record time
///    per call, and Reset() -> LastRegisterForCounter() copies the last
///    recorded counts into every record time not reached. Late in the stage,
///    when reactions are rare, the late record times were therefore stale
///    (validation: e_aq with bulk O2 frozen from ~800 ns). Here a snapshot is
///    taken as soon as any step passes a record time; species counts change
///    only on reactions, so it is off by at most one reaction.
/// 2. Debug-level molecule bookkeeping. A diffusive jump never changes the
///    mesh total and one reaction changes it by at most 2 (A + B -> nothing),
///    so between two Gillespie steps the total may only move by 0, +-1 or +-2;
///    anything else, or a change across a mesh coarsening (EndOfMesh -> next
///    BeginOfMesh, no step in between), is a drift.
class MeshTotalsAction : public G4UserMeshAction
{
public:
  void ResetEvent(long long handOverTotal)
  {
    fLastTotal = handOverTotal;
    fDrifts = 0;
    fMeshChanges = 0;
    fFirstMesh = true;
  }

  /// Record times of this event (sorted ascending); clears earlier records
  /// and the spatial snapshots. Reads /chem/meso/spatialOutput (PreInit).
  void StartRecording(std::vector<G4double> recordTimes)
  {
    fRecordTimes = std::move(recordTimes);
    fNextRecord = 0;
    fRecords.clear();
    fSpatialOn = MesoSettings::Current().spatialOutput;
    ClearSpatial();
  }

  /// Fills every record time not yet passed with the final state of the
  /// stage, which holds from the last Gillespie step to the end time.
  void FinishRecording(const G4DNAMesh *mesh)
  {
    if (mesh == nullptr || fNextRecord >= fRecordTimes.size()) return;
    const MeshCounts counts = MeshSpeciesCounts(*mesh);
    const std::size_t snapshot = fSpatialOn ? TakeSnapshot(*mesh) : 0;
    for (; fNextRecord < fRecordTimes.size(); ++fNextRecord) {
      fRecords[fRecordTimes[fNextRecord]] = counts;
      if (fSpatialOn) fSpatial.records.push_back({fRecordTimes[fNextRecord] / ns, snapshot});
    }
  }

  const std::map<G4double, MeshCounts> &Records() const { return fRecords; }

  /// Spatial snapshots of this event (empty unless /chem/meso/spatialOutput).
  MesoSpatialFile::EventData &SpatialData() { return fSpatial; }
  void ClearSpatial() { fSpatial = MesoSpatialFile::EventData{}; }

  /// Species columns of the spatial file: every molecular configuration but
  /// water and the bulk species (as in SpeciesMeso.*), by display name,
  /// sorted. Built once per thread, on first use (the molecule table is
  /// complete after /run/initialize).
  const std::vector<std::string> &SpatialSpecies()
  {
    if (!fSpeciesBuilt) BuildSpecies();
    return fSpecies;
  }

  void BeginOfMesh(const G4VDNAMesh *aMesh, G4double time) override
  {
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    Record(*mesh, time);
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const long long total = MeshTotal(*mesh);
    const G4bool conserved = (total == fLastTotal);
    if (!conserved) ++fDrifts;
    if (!fFirstMesh) ++fMeshChanges;
    DnaLogger::Print(DnaLogger::Level::Debug,
                     G4String("[meso] ") + (fFirstMesh ? "first mesh" : "mesh change") +
                       " at t = " + Format(time / ns) + " ns: pixels = " +
                       std::to_string(MeshPixels(*mesh)) + ", cell = " +
                       Format(mesh->GetResolution() / nm) + " nm, occupied cells = " +
                       std::to_string(mesh->const_end() - mesh->const_begin()) +
                       ", total = " + std::to_string(total) + " (previous " +
                       std::to_string(fLastTotal) + (conserved ? ", conserved)" : ", DRIFT)"));
    fFirstMesh = false;
    fLastTotal = total;
    fMeshStartTotal = total;
    fSteps = 0;
    fChangedSteps = 0;
    fAnomalousSteps = 0;
  }

  void InMesh(const G4VDNAMesh *aMesh, G4double time) override
  {
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    Record(*mesh, time);
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const long long total = MeshTotal(*mesh);
    const long long delta = total - fLastTotal;
    ++fSteps;
    if (delta != 0) ++fChangedSteps;
    if (delta > 2 || delta < -2) {
      ++fAnomalousSteps;
      ++fDrifts;
    }
    fLastTotal = total;
  }

  void EndOfMesh(const G4VDNAMesh *aMesh, G4double time) override
  {
    if (!DnaLogger::Enabled(DnaLogger::Level::Debug)) return;
    const auto *mesh = dynamic_cast<const G4DNAMesh *>(aMesh);
    if (mesh == nullptr) return;
    const long long total = MeshTotal(*mesh);
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] end of mesh at t = " + Format(time / ns) +
                       " ns: pixels = " + std::to_string(MeshPixels(*mesh)) + ", total = " +
                       std::to_string(total) + " (start " + std::to_string(fMeshStartTotal) +
                       "), steps = " + std::to_string(fSteps) +
                       ", reaction steps changing the total = " +
                       std::to_string(fChangedSteps) +
                       ", steps with |delta| > 2 = " + std::to_string(fAnomalousSteps));
    fLastTotal = total;
  }

  long long Drifts() const { return fDrifts; }
  long long MeshChanges() const { return fMeshChanges; }
  long long LastTotal() const { return fLastTotal; }

private:
  /// Snapshots the mesh for every record time the stage time has reached.
  /// Several record times passed in one call share one spatial snapshot.
  void Record(const G4DNAMesh &mesh, G4double time)
  {
    if (fNextRecord >= fRecordTimes.size() || time < fRecordTimes[fNextRecord]) return;
    const MeshCounts counts = MeshSpeciesCounts(mesh);
    const std::size_t snapshot = fSpatialOn ? TakeSnapshot(mesh) : 0;
    for (; fNextRecord < fRecordTimes.size() && time >= fRecordTimes[fNextRecord];
         ++fNextRecord) {
      fRecords[fRecordTimes[fNextRecord]] = counts;
      if (fSpatialOn) fSpatial.records.push_back({fRecordTimes[fNextRecord] / ns, snapshot});
    }
  }

  /// A species column: not water (any state: H2O, and the excited/ionised
  /// H2O^-1/^0/^1 of the dissociation channels, which never reach the mesh),
  /// not G4FakeMolecule ("None"), not a bulk "(B)" pseudo-species.
  static bool IsSpatialSpecies(const G4MolecularConfiguration *conf)
  {
    const auto *definition = conf->GetDefinition();
    if (definition == G4H2O::Definition() || definition == G4FakeMolecule::Definition()) {
      return false;
    }
    return !G4StrUtil::ends_with(conf->GetUserID(), "(B)");
  }

  void BuildSpecies()
  {
    fSpeciesBuilt = true;
    fSpecies.clear();
    fColumnByName.clear();
    fColumnByConf.clear();
    auto *table = G4MoleculeTable::Instance();
    // G4MoleculeIterator::operator() returns true once even on an empty map.
    if (table->GetNumberOfDefinedSpecies() > 0) {
      auto it = table->GetConfigurationIterator();
      while (it()) {
        if (!IsSpatialSpecies(it.value())) continue;
        fSpecies.push_back(it.value()->GetName());
      }
    }
    std::sort(fSpecies.begin(), fSpecies.end());
    // Two configurations with one display name share a column (as in SpeciesMeso.*).
    fSpecies.erase(std::unique(fSpecies.begin(), fSpecies.end()), fSpecies.end());
    for (std::size_t i = 0; i < fSpecies.size(); ++i) fColumnByName[fSpecies[i]] = i;
  }

  /// Column of a mesh species, or -1 (logged once per species and thread)
  /// when it is not in the column list.
  long Column(const G4MolecularConfiguration *conf)
  {
    const auto cached = fColumnByConf.find(conf);
    if (cached != fColumnByConf.end()) return cached->second;
    long column = -1;
    const G4String &userID = conf->GetUserID();
    if (IsSpatialSpecies(conf)) {
      const auto byName = fColumnByName.find(conf->GetName());
      if (byName != fColumnByName.end()) column = static_cast<long>(byName->second);
    }
    if (column < 0) {
      DnaLogger::Print(DnaLogger::Level::Warning,
                       "[meso] spatial output: mesh species '" + conf->GetName() +
                         "' (user ID '" + userID +
                         "') is not in the species column list; skipped");
    }
    fColumnByConf[conf] = column;
    return column;
  }

  /// Appends one sparse spatial snapshot of the mesh (only cells whose
  /// species-column counts sum to > 0; none gives N = 0) and returns its index.
  std::size_t TakeSnapshot(const G4DNAMesh &mesh)
  {
    if (!fSpeciesBuilt) BuildSpecies();
    const std::size_t nSpecies = fSpecies.size();
    MesoSpatialFile::Snapshot snapshot;
    snapshot.cellSize_nm = mesh.GetResolution() / nm;
    std::vector<std::uint32_t> row(nSpecies, 0);
    for (auto it = mesh.const_begin(); it != mesh.const_end(); ++it) {
      std::fill(row.begin(), row.end(), 0u);
      std::uint64_t sum = 0;
      for (const auto &[molType, count] : std::get<2>(*it)) {
        const long column = Column(molType);
        if (column < 0) continue;
        row[static_cast<std::size_t>(column)] += static_cast<std::uint32_t>(count);
        sum += static_cast<std::uint32_t>(count);
      }
      if (sum == 0) continue;
      const G4ThreeVector centre = std::get<1>(*it).middlePoint() / nm;
      snapshot.position_nm.push_back(centre.x());
      snapshot.position_nm.push_back(centre.y());
      snapshot.position_nm.push_back(centre.z());
      snapshot.counts.insert(snapshot.counts.end(), row.begin(), row.end());
    }
    fSpatial.snapshots.push_back(std::move(snapshot));
    return fSpatial.snapshots.size() - 1;
  }

  std::vector<G4double> fRecordTimes;
  std::size_t fNextRecord = 0;
  std::map<G4double, MeshCounts> fRecords;

  /// Spatial snapshots (/chem/meso/spatialOutput), one per Record() /
  /// FinishRecording() call that passes at least one record time.
  G4bool fSpatialOn = false;
  MesoSpatialFile::EventData fSpatial;
  G4bool fSpeciesBuilt = false;
  std::vector<std::string> fSpecies;
  std::map<std::string, std::size_t> fColumnByName;
  std::map<const G4MolecularConfiguration *, long> fColumnByConf;

  long long fLastTotal = 0;
  long long fMeshStartTotal = 0;
  long long fSteps = 0;
  long long fChangedSteps = 0;
  long long fAnomalousSteps = 0;
  long long fDrifts = 0;
  long long fMeshChanges = 0;
  G4bool fFirstMesh = true;
};

/// Thread-local: one TimeStepAction (and so one mesh action) per thread. The
/// scheduler owns the action; this is a non-owning view for the summary.
G4ThreadLocal MeshTotalsAction *tMeshAction = nullptr;
}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::TimeStepAction(const G4VChemistryWorld *chemistryWorld)
  : G4UserTimeStepAction(),
    fpChemWorld(chemistryWorld),
    fpEventScheduler(std::make_unique<G4DNAEventScheduler>())
{
  // User time steps (SBS only) are added in StartProcessing: in Serial mode
  // this constructor runs before the macro selects the time-step model.
  if (fpChemWorld == nullptr) {
    G4Exception("TimeStepAction::TimeStepAction", "NoChemistryWorld", FatalException,
                "The mesoscopic stage needs the chemistry world (its boundary is the mesh).");
  }
  auto meshAction = std::make_unique<MeshTotalsAction>();
  tMeshAction = meshAction.get();
  fpEventScheduler->SetUserMeshAction(std::move(meshAction));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

TimeStepAction::~TimeStepAction()
{
  tMeshAction = nullptr;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::StartProcessing()
{
  fHandedOver = false;
  const G4bool sbs = ChemUtils::GetCurrentTimeStepModel() == G4ChemTimeStepModel::SBS;
  fMesoOn = MesoSettings::StageEnabled(MesoSettings::Current(), sbs);

  if (sbs && !fSbsTimeStepsAdded) {
    // Minimum time steps of the SBS stepper (chem1-chem6 pattern): e.g. from
    // 1 ps to 10 ps the step returned is at least 1 ps, unless a reaction or
    // an interaction with the medium needs a shorter one. Thread-local
    // scheduler, so once per thread. Not for IRT_syn, whose stepper keeps
    // its own minimum (G4DNAIndependentReactionTimeStepper), as in the UHDR
    // example.
    AddTimeStep(1 * picosecond, 0.1 * picosecond);
    AddTimeStep(10 * picosecond, 1 * picosecond);
    AddTimeStep(100 * picosecond, 10 * picosecond);
    AddTimeStep(1000 * picosecond, 100 * picosecond);
    AddTimeStep(10000 * picosecond, 1000 * picosecond);
    fSbsTimeStepsAdded = true;
  }

  fParticleStageWall = 0.;
  if (!fMesoOn) {
    // No mesoscopic stage: the particle-based stage runs to the end time and
    // the event scheduler is never used (no record times, no counter).
    fHandOverDrift = false;
    if (DnaLogger::Enabled(DnaLogger::Level::Info)) {
      const G4Event* event = G4EventManager::GetEventManager()->GetConstCurrentEvent();
      DnaLogger::Print(DnaLogger::Level::Info,
                       "[TimeStepAction] Chemistry starts (particle-based stage only), event " +
                         std::to_string(event != nullptr ? event->GetEventID() : -1));
    }
    fChemTimer.Start();
    return;
  }

  fpEventScheduler->SetVerbose(G4Scheduler::Instance()->GetVerbose());

  // G4DNAEventScheduler::RecordTime / LastRegisterForCounter dereference the
  // record-time iterator unconditionally, so at least one record time is
  // required. Log grid from the hand-over time to the end time
  // (/chem/meso/timesPerDecade); ResetCounter rewinds it. When the end time is
  // not after the hand-over time there is no mesoscopic stage and the grid
  // reduces to the end time alone.
  const auto& meso = MesoSettings::Current();
  const G4double handOverTime = meso.handOverTime * ns;
  // The species output (SpeciesMeso.*) is recorded by the mesh action on the
  // same grid (see MeshTotalsAction); the scheduler's own counter map is kept
  // fed only because its RecordTime() needs at least one record time.
  const G4double endTime = G4Scheduler::Instance()->GetEndTime();
  std::vector<G4double> recordTimes;
  if (handOverTime < endTime) {
    for (const G4double t : MesoSettings::LogTimeGrid(handOverTime, endTime, meso.timesPerDecade)) {
      fpEventScheduler->AddTimeToRecord(t);
      recordTimes.push_back(t);
    }
  }
  else {
    fpEventScheduler->AddTimeToRecord(endTime);
  }
  if (tMeshAction != nullptr) tMeshAction->StartRecording(std::move(recordTimes));
  fpEventScheduler->ResetCounter();
  fHandOverDrift = false;
  if (DnaLogger::Enabled(DnaLogger::Level::Info)) {
    const G4Event* event = G4EventManager::GetEventManager()->GetConstCurrentEvent();
    DnaLogger::Print(DnaLogger::Level::Info,
                     "[TimeStepAction] Chemistry starts, event " +
                       std::to_string(event != nullptr ? event->GetEventID() : -1));
  }
  fChemTimer.Start();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPreTimeStepAction()
{
  // Particle-stage entries of the scheduler's record-time counter (UHDR).
  if (fMesoOn) fpEventScheduler->ParticleBasedCounter();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::UserPostTimeStepAction()
{
  // Meso off (/chem/meso/enable false, or SBS): never hand over, the
  // particle-based stage runs to the end time.
  if (fMesoOn && !fHandedOver &&
      G4Scheduler::Instance()->GetGlobalTime() >= MesoSettings::Current().handOverTime * ns) {
    fHandedOver = true;
    CompartmentBased();
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int TimeStepAction::InitialPixel() const
{
  const G4double side = 2. * fpChemWorld->GetChemistryBoundary()->halfSideLengthInX();
  return MesoSettings::PixelCount(side, MesoSettings::Current().voxelSize * mm);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::MergeScavengerSpeciesIntoBulk(G4double globalTime)
{
  // G4DNAEventScheduler::Voxelizing leaves out (neither meshes nor kills) every
  // track whose species is held by the scavenger material: bulk O2 and
  // radiolytic O2 share the "O2" configuration (ADR 0004), so with a bulk O2
  // concentration set, the radiolytic O2 tracks alive at the hand-over stayed
  // alive in the particle stage. On the scheduler's next step IRT_syn then
  // paired them with the killed (meshed) tracks, and
  // G4DNAIndependentReactionTimeStepper::FindReaction loops forever on a
  // reaction whose partner is fStopAndKill (it skips it without removing it).
  // Fix: merge these molecules into the bulk pool, as G4DNAScavengerProcess
  // does for a scavenger-species product (ADR 0004), and kill them before
  // Voxelizing, whose CleanAllReaction() then drops their pending reactions
  // too, so no live track survives the hand-over.
  auto *scavenger =
    dynamic_cast<G4DNAScavengerMaterial *>(G4Scheduler::Instance()->GetScavengerMaterial());
  if (scavenger == nullptr) return;
  long long merged = 0;
  for (auto *track : *G4ITTrackHolder::Instance()->GetMainList()) {
    if (track->GetTrackStatus() == fStopAndKill) continue;
    auto *molType = GetMolecule(track)->GetMolecularConfiguration();
    if (!scavenger->find(molType)) continue;
    scavenger->AddNumberMoleculePerVolumeUnitForMaterialConf(molType, globalTime);
    track->SetTrackStatus(fStopAndKill);
    ++merged;
  }
  if (merged > 0) {
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] hand-over at t = " + Format(globalTime / ns) + " ns: " +
                       std::to_string(merged) +
                       " tracked scavenger-species molecule(s) merged into the bulk pool");
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::CompartmentBased()
{
  const G4double globalTime = G4Scheduler::Instance()->GetGlobalTime();
  const G4int pixel = InitialPixel();
  const G4bool debug = DnaLogger::Enabled(DnaLogger::Level::Debug);

  long long handOverTotal = 0;
  if (debug) {
    // Same selection as G4DNAEventScheduler::Voxelizing: every main-list
    // track except species held by the scavenger material.
    auto *scavenger =
      dynamic_cast<G4DNAScavengerMaterial *>(G4Scheduler::Instance()->GetScavengerMaterial());
    long long listed = 0;
    long long killed = 0;
    for (auto *track : *G4ITTrackHolder::Instance()->GetMainList()) {
      auto *molType = GetMolecule(track)->GetMolecularConfiguration();
      if (scavenger != nullptr && scavenger->find(molType)) continue;
      ++listed;
      if (track->GetTrackStatus() == fStopAndKill) ++killed;
    }
    handOverTotal = listed;
    const G4double nextTime = G4ITTrackHolder::Instance()->GetNextTime();
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] hand-over at t = " + Format(globalTime / ns) +
                       " ns: particle-stage total = " + std::to_string(listed) + " (" +
                       std::to_string(killed) + " already killed), initial pixels = " +
                       std::to_string(pixel) + ", cell = " +
                       Format(2. * fpChemWorld->GetChemistryBoundary()->halfSideLengthInX() /
                              pixel / nm) +
                       " nm, delayed tracks pending: " +
                       (nextTime == DBL_MAX ? G4String("none")
                                            : "next at " + Format(nextTime / ns) + " ns"));
  }
  MergeScavengerSpeciesIntoBulk(globalTime);
  fChemTimer.Stop();
  fParticleStageWall = fChemTimer.GetRealElapsed();
  fChemTimer.Start();

  // The G4MoleculeCounter (Species.*) covers the particle-based stage only,
  // so it is muted from here to EndProcessing (the manager is thread-local:
  // this only affects this thread; the UHDR example has no counter). Two
  // calls would otherwise trip its time-consistency check (TIME_DONT_MATCH):
  // - G4DNAUpdateSystemModel feeds it at the mesoscopic time while
  //   G4Scheduler's global time stays at the hand-over;
  // - the tracks Voxelizing marks fStopAndKill are deleted on the scheduler's
  //   next step, once its global time has jumped to the end time, and
  //   ~G4Molecule removes them at their own (hand-over) time.
  auto *counterManager = G4MoleculeCounterManager::Instance();
  fCounterWasActive = counterManager->GetIsActive();
  fCounterMuted = true;
  counterManager->SetIsActive(false);

  // UHDR example, TimeStepAction::CompartmentBased.
  fpEventScheduler->SetStartTime(globalTime);  // continue from the global time
  fpEventScheduler->SetChangeMesh(true);
  fpEventScheduler->Initialize(*fpChemWorld->GetChemistryBoundary(), pixel);

  if (debug) {
    const auto *mesh = fpEventScheduler->GetMesh();
    const long long total = MeshTotal(*mesh);
    DnaLogger::Print(DnaLogger::Level::Debug,
                     "[meso] after Initialize: pixels = " +
                       std::to_string(fpEventScheduler->GetPixels()) + ", cell = " +
                       Format(mesh->GetResolution() / nm) + " nm, occupied cells = " +
                       std::to_string(mesh->const_end() - mesh->const_begin()) +
                       ", mesh total = " + std::to_string(total) +
                       (total == handOverTotal ? " (conserved)" : " (DRIFT)") +
                       ", meso end time = " + Format(fpEventScheduler->GetEndTime() / ns) +
                       " ns");
    // The hand-over itself must not change the total; the mesh action then
    // tracks the mesh from this total on.
    fHandOverDrift = (total != handOverTotal);
    if (tMeshAction != nullptr) tMeshAction->ResetEvent(total);
  }

  const auto bulkAtHandOver = debug ? BulkCounts() : decltype(BulkCounts()){};

  fpEventScheduler->Run();
  // The state after the last Gillespie step holds up to the end time.
  if (tMeshAction != nullptr) {
    tMeshAction->FinishRecording(fpEventScheduler->GetMesh());
    WriteSpatialSnapshots();
  }

  if (debug) {
    // Net change of each bulk species over the mesoscopic stage (reactions
    // against a bulk partner consume it; bulk products, e.g. O2 from
    // O3- + H3O+(B) with bulk O2 set, add to it). H2O(B) / H3Op(B) / OHm(B)
    // stay constant: G4DNAScavengerMaterial holds the pH fixed.
    for (const auto &[conf, after] : BulkCounts()) {
      long long before = after;
      for (const auto &[confAtHandOver, n] : bulkAtHandOver) {
        if (confAtHandOver == conf) before = n;
      }
      DnaLogger::Print(DnaLogger::Level::Debug,
                       "[meso] bulk " + conf->GetName() + ": " + std::to_string(before) +
                         " at hand-over -> " + std::to_string(after) +
                         " at end of mesoscopic stage (net " + std::to_string(after - before) +
                         ")");
    }
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Appends this event's spatial snapshots (/chem/meso/spatialOutput) to the
// staged SpeciesMesoSpatial.h5 (<outdir>/.pending_meso_spatial/).
void TimeStepAction::WriteSpatialSnapshots()
{
  if (tMeshAction == nullptr) return;
  auto &data = tMeshAction->SpatialData();
  if (data.records.empty()) return;
  const G4Run *run = G4RunManager::GetRunManager()->GetCurrentRun();
  const G4Event *event = G4EventManager::GetEventManager()->GetConstCurrentEvent();
  data.runId = run != nullptr ? run->GetRunID() : 0;
  data.eventId = event != nullptr ? event->GetEventID() : 0;
  std::string err;
  if (!MesoSpatialFile::AppendEvent(MesoSpatialFile::StagedPath(OutputDir::GetDirectory()),
                                    tMeshAction->SpatialSpecies(), data, err)) {
    G4Exception("TimeStepAction::WriteSpatialSnapshots", "MesoSpatialWriteFailed",
                JustWarning,
                ("Spatial snapshots of run " + std::to_string(data.runId) + ", event " +
                 std::to_string(data.eventId) + " not written: " + err)
                  .c_str());
  }
  tMeshAction->ClearSpatial();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Reads the mesh action's record-time snapshots (not the scheduler's counter
// map, see MeshTotalsAction), then ResetCounter() for the next event.
void TimeStepAction::CollectMesoSpecies()
{
  if (tMeshAction == nullptr) return;
  for (const auto &[time, counts] : tMeshAction->Records()) {
    for (const auto &[molType, n] : counts) {
      // Bulk species (G4DNAScavengerMaterial: H2O(B), H3Op(B), OHm(B), the
      // /chem/env/scavenger species) and water are not scored, as in Species.*.
      const G4String &userID = molType->GetUserID();
      if (userID == "H2O" || G4StrUtil::ends_with(userID, "(B)")) continue;
      // Display name as in Species.Txt (G4MolecularConfiguration::GetName).
      fMesoSpeciesCounter.Add(time / ns, molType->GetName(), n);
    }
  }
  fpEventScheduler->ResetCounter();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Counts each bimolecular reaction that fires, binned by time, so a
// per-reaction rate-over-time output can be written at end of run (see
// Run::Merge / RunAction::EndOfRunAction). Only the particle-based stage
// reports reactions; the mesoscopic stage has no reaction hook.
void TimeStepAction::UserReactionAction(const G4Track &a, const G4Track &b,
                                        const std::vector<G4Track *> * /*products*/)
{
  const auto *molA = GetMolecule(a)->GetMolecularConfiguration();
  const auto *molB = GetMolecule(b)->GetMolecularConfiguration();

  const auto *reactionData =
      G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(molA, molB);
  if (reactionData == nullptr) {
    DnaLogger::Print(DnaLogger::Level::Warning,
                     "[TimeStepAction] reaction fired with no matching reaction-table entry: "
                     + molA->GetName() + " + " + molB->GetName());
    return;
  }

  const G4String& label = ReactionTableDump::LabelFor(reactionData->GetReactionID());
  fReactionCounter.Record(label, G4Scheduler::Instance()->GetGlobalTime());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void TimeStepAction::EndProcessing()
{
  fChemTimer.Stop();
  if (DnaLogger::Enabled(DnaLogger::Level::Info)) {
    const G4double stageWall = fChemTimer.GetRealElapsed();
    G4String summary = "[TimeStepAction] Chemistry ends at global time " +
                       Format(G4Scheduler::Instance()->GetGlobalTime() / picosecond) +
                       " ps, wall time " + Format(fParticleStageWall + stageWall) + " s";
    if (fHandedOver) {
      summary += " (particle stage " + Format(fParticleStageWall) + " s, meso " +
                 Format(stageWall) + " s)";
      if (DnaLogger::Enabled(DnaLogger::Level::Debug) && tMeshAction != nullptr) {
        const G4bool ok = !fHandOverDrift && tMeshAction->Drifts() == 0;
        summary += "; meso: " + std::to_string(tMeshAction->MeshChanges()) +
                   " mesh changes, final total = " + std::to_string(tMeshAction->LastTotal()) +
                   ", conservation " + (ok ? "OK" : "DRIFT");
      }
    }
    DnaLogger::Print(DnaLogger::Level::Info, summary);
  }
  if (fCounterMuted) {
    // The killed particle-stage tracks are gone by now (see CompartmentBased).
    G4MoleculeCounterManager::Instance()->SetIsActive(fCounterWasActive);
    fCounterMuted = false;
  }
  if (!fMesoOn) {
    // The event scheduler was not set up for this event (StartProcessing).
    fHandedOver = false;
    fHandOverDrift = false;
    return;
  }
  fpEventScheduler->Reset();
  if (DnaLogger::Enabled(DnaLogger::Level::Debug) && tMeshAction != nullptr) {
    for (const auto &[time, counts] : tMeshAction->Records()) {
      long long total = 0;
      for (const auto &[molType, n] : counts) total += n;
      DnaLogger::Print(DnaLogger::Level::Debug, "[meso] record t = " + Format(time / ns) +
                                                  " ns: total = " + std::to_string(total));
    }
  }
  CollectMesoSpecies();
  fHandedOver = false;
  fHandOverDrift = false;
}
