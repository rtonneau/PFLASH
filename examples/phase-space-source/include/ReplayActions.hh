// ReplayActions: the user actions of phsp-replay (stage 2).
//
// One event replays one Phase-space history (PhaseSpaceSource). The stage-2
// deposit of a history is the energy deposited in steps whose pre-step point
// is in the box: the stage-1 in-box deposit has the same definition
// (RecordSteppingAction), so over the same histories the two agree in the mean
// (ADR 0014: a re-entry into the box is a record of its own, replayed as a
// primary, so every in-box track segment of stage 1 is either replayed or
// regenerated inside the box). Every track is killed at the end of the step
// that leaves the box; a track found outside the box (a primary located in
// the world, which should not happen, or a secondary born outside) is killed
// at its first step and counted; ReplayTrackingAction kills a track that
// starts outside the box before its first step, so that no physics track ever
// steps in the water margin around the box (ReplayDetectorConstruction).
//
// Chemistry (ticket 04, /phsp/chem/enable true): every record of a history is
// a primary of the same event, so all their molecules enter one chemistry
// stage. Within an event the physics comes first: the DNA models only create
// the H2O and e_aq tracks of the pre-chemical stage (G4DNAChemistryManager::
// CreateWaterMolecule / CreateSolvatedElectron build and stack them, they draw
// no random number), and ReplayStackingAction starts the chemistry
// (G4DNAChemistryManager::Run) once the physics stack is empty. The H2O
// dissociation (G4DNAMolecularDissociation + G4DNAWaterDissociationDisplacer)
// and every reaction run inside that chemistry stage. So the chemistry
// settings (pH, O2, end time) cannot change the physics of a history. The
// chemistry stage itself is not bit-reproducible from one process to the
// next (G4DNAScavengerProcess orders its bulk partners by pointer; README.md,
// "Chemistry reproducibility").
//   - Pre-chemical counts: a PreChemicalCounter (G4VPhysChemIO) per event
//     counts the products the physics hands to the chemistry (ionised,
//     excited and attached H2O, solvated electrons); ReplayTimeStepAction
//     takes them at G4UserTimeStepAction::StartProcessing, before the first
//     chemistry step and so before any chemistry random number.
//   - Species: one G4MoleculeCounter per worker (H2O ignored, 1 ps time
//     precision, reset before each event, as chem4-chem6), read at the end of
//     the event on the SpeciesTimeGrid of that worker's G4Scheduler end time.
//     Molecules diffuse in box + water margin and are counted wherever they
//     are; the ones killed outside water are counted per history
//     (MoleculesKilledOutsideWater, expected 0).
//
// MT: each worker pushes one ReplayHistoryRow per event into the process-wide
// ReplayResults (mutex-protected); phsp-replay's main() sorts the rows by
// global index before writing Histories.csv, so the file does not depend on
// the thread count or on how the range was split into runs.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
#include "G4UserStackingAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4UserTimeStepAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4VPhysChemIO.hh"
#include "G4VUserActionInitialization.hh"
#include "globals.hh"

class G4LogicalVolume;

namespace phsp
{

class ReplayDetectorConstruction;
class ReplaySource;
struct ReplayChemistrySettings;

// The products of the physical stage handed to the chemistry (one history).
struct PreChemicalCounts
{
  std::uint64_t ionisedWater = 0;   // H2O+ (eIonizedMolecule)
  std::uint64_t excitedWater = 0;   // H2O* (eExcitedMolecule)
  std::uint64_t attachedWater = 0;  // H2O- (eDissociativeAttachment)
  std::uint64_t solvatedElectrons = 0;  // e_aq
  // Of the above, the ones whose parent track was outside the box (expected 0:
  // only the box has DNA models, and tracks outside it are killed).
  std::uint64_t outsideBox = 0;
};

// One species of one history: counts on the history's time grid.
struct SpeciesSeries
{
  std::string name;  // G4MolecularConfiguration::GetName()
  std::vector<int> counts;
};

// One replayed history: Histories.csv row (and its Species.csv rows).
struct ReplayHistoryRow
{
  std::uint64_t globalIndex = 0;
  std::uint64_t fileId = 0;
  std::uint64_t historyIndex = 0;
  std::uint64_t stage1EventId = 0;
  std::int32_t boxIx = 0;
  std::int32_t boxIy = 0;
  std::uint64_t recordCount = 0;
  double stage1DepositEv = 0;  // stage-1 in-box deposit (history table)
  double stage2DepositEv = 0;
  std::uint64_t tracksKilledOutsideBox = 0;
  // Chemistry only.
  PreChemicalCounts preChemical;
  double chemistryEndTimeNs = 0;          // this worker's G4Scheduler end time
  std::uint64_t moleculesKilledOutsideWater = 0;
  std::vector<double> speciesTimesNs;     // SpeciesTimeGrid(end time)
  std::vector<SpeciesSeries> species;     // sorted by name
};

// Process-wide store of the replayed histories, shared by every thread.
class ReplayResults
{
 public:
  void Add(ReplayHistoryRow row);
  // All rows so far, sorted by global index (call when no run is active).
  std::vector<ReplayHistoryRow> SortedRows() const;
  void AddWallTime(double seconds) { fWallTimeS += seconds; }
  double WallTimeS() const { return fWallTimeS; }

 private:
  mutable std::mutex fMutex;
  std::vector<ReplayHistoryRow> fRows;
  double fWallTimeS = 0;  // master only
};

// Counts the pre-chemical products of one event (G4VPhysChemIO: the
// chemistry manager calls it from the DNA models, on the event's thread).
class PreChemicalCounter : public G4VPhysChemIO
{
 public:
  explicit PreChemicalCounter(G4double boxHalfSide) : fBoxHalfSide(boxHalfSide) {}
  void InitializeFile() override {}
  void NewRun() override {}
  void NewEvent() override {}
  void CreateWaterMolecule(G4int modification, G4int level, G4double energy, const G4Track* track) override;
  void CreateSolvatedElectron(const G4Track* track, G4ThreeVector* finalPosition = nullptr) override;
  void WriteInto(const G4String&, std::ios_base::openmode = std::ios_base::out) override {}
  void CloseFile() override {}
  const PreChemicalCounts& Counts() const { return fCounts; }

 private:
  void CountPosition(const G4Track* track);
  G4double fBoxHalfSide;
  PreChemicalCounts fCounts;
};

class ReplayEventAction : public G4UserEventAction
{
 public:
  // `chemistry`: build the per-event chemistry scoring (counter, species).
  ReplayEventAction(ReplayResults& results, const ReplayDetectorConstruction& detector, G4bool chemistry,
                    G4int moleculeCounterId)
    : fResults(results), fDetector(detector), fChemistry(chemistry), fMoleculeCounterId(moleculeCounterId)
  {}
  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;
  void AddDeposit(G4double deposit) { fDeposit += deposit; }
  const ReplayDetectorConstruction& Detector() const { return fDetector; }
  void CountTrackOutsideBox() { ++fTracksOutside; }
  // ReplayTimeStepAction::StartProcessing: the chemistry starts.
  void TakePreChemicalSnapshot();

 private:
  void FillSpecies(ReplayHistoryRow& row) const;

  ReplayResults& fResults;
  const ReplayDetectorConstruction& fDetector;
  G4bool fChemistry;
  G4int fMoleculeCounterId;
  G4double fDeposit = 0;
  std::uint64_t fTracksOutside = 0;
  PreChemicalCounter* fPreChemical = nullptr;  // owned by the chemistry manager (SetPhysChemIO)
  G4bool fHaveSnapshot = false;
  PreChemicalCounts fSnapshot;
};

class ReplaySteppingAction : public G4UserSteppingAction
{
 public:
  ReplaySteppingAction(const ReplayDetectorConstruction& detector, ReplayEventAction& eventAction)
    : fDetector(detector), fEventAction(eventAction)
  {}
  void UserSteppingAction(const G4Step* step) override;

 private:
  const ReplayDetectorConstruction& fDetector;
  ReplayEventAction& fEventAction;
};

// Kills, before its first step, a physics track that starts outside the box
// (located in the margin or the world), and counts it as a track outside the
// box. Molecules are not tracked by the G4TrackingManager, so this does not
// touch them.
class ReplayTrackingAction : public G4UserTrackingAction
{
 public:
  explicit ReplayTrackingAction(ReplayEventAction& eventAction) : fEventAction(eventAction) {}
  void PreUserTrackingAction(const G4Track* track) override;

 private:
  ReplayEventAction& fEventAction;
};

// Starts the chemistry stage when the physics stack is empty (chem1-chem6, UHDR).
class ReplayStackingAction : public G4UserStackingAction
{
 public:
  void NewStage() override;
};

// Takes the pre-chemical snapshot when the chemistry starts.
class ReplayTimeStepAction : public G4UserTimeStepAction
{
 public:
  explicit ReplayTimeStepAction(ReplayEventAction& eventAction) : fEventAction(eventAction) {}
  void StartProcessing() override { fEventAction.TakePreChemicalSnapshot(); }

 private:
  ReplayEventAction& fEventAction;
};

class ReplayRunAction : public G4UserRunAction
{
 public:
  // `source`, `results` and `chemistry` are null on worker threads. At the
  // start of every real run (the BeamOn(0) of /run/initialize is a fake run,
  // which returns before BeginOfRunAction), so before any event, the master
  // checks that the chemistry is active exactly when /phsp/chem/enable says
  // so: a /run/initialize issued from a nested macro (/control/execute) would
  // skip ReplayPhysicsList::EnableChemistry. With the chemistry on it also
  // warns (JustWarning) when the diffusion length of the fastest species,
  // sqrt(6 D_max t_end), exceeds a third of the water margin.
  ReplayRunAction(ReplaySource* source, ReplayResults* results, const ReplayChemistrySettings* chemistry)
    : fSource(source), fResults(results), fChemistry(chemistry)
  {}
  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;

 private:
  ReplaySource* fSource;
  ReplayResults* fResults;
  const ReplayChemistrySettings* fChemistry;
  std::chrono::steady_clock::time_point fStart;
};

class ReplayActionInitialization : public G4VUserActionInitialization
{
 public:
  ReplayActionInitialization(ReplaySource& source, ReplayResults& results, const ReplayDetectorConstruction& detector,
                             const ReplayChemistrySettings& chemistry)
    : fSource(source), fResults(results), fDetector(detector), fChemistry(chemistry)
  {}
  void BuildForMaster() const override;
  void Build() const override;

 private:
  ReplaySource& fSource;
  ReplayResults& fResults;
  const ReplayDetectorConstruction& fDetector;
  const ReplayChemistrySettings& fChemistry;
};

}  // namespace phsp
