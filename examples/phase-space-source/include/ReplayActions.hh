// ReplayActions: the user actions of phsp-replay (stage 2, physics only).
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
// at its first step and counted.
//
// MT: each worker pushes one ReplayHistoryRow per event into the process-wide
// ReplayResults (mutex-protected); phsp-replay's main() sorts the rows by
// global index before writing Histories.csv, so the file does not depend on
// the thread count or on how the range was split into runs.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VUserActionInitialization.hh"
#include "globals.hh"

class G4LogicalVolume;

namespace phsp
{

class ReplayDetectorConstruction;
class ReplaySource;

// One replayed history: Histories.csv row.
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
};

// Process-wide store of the replayed histories, shared by every thread.
class ReplayResults
{
 public:
  void Add(const ReplayHistoryRow& row);
  // All rows so far, sorted by global index (call when no run is active).
  std::vector<ReplayHistoryRow> SortedRows() const;
  void AddWallTime(double seconds) { fWallTimeS += seconds; }
  double WallTimeS() const { return fWallTimeS; }

 private:
  mutable std::mutex fMutex;
  std::vector<ReplayHistoryRow> fRows;
  double fWallTimeS = 0;  // master only
};

class ReplayEventAction : public G4UserEventAction
{
 public:
  explicit ReplayEventAction(ReplayResults& results) : fResults(results) {}
  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;
  void AddDeposit(G4double deposit) { fDeposit += deposit; }
  void CountTrackOutsideBox() { ++fTracksOutside; }

 private:
  ReplayResults& fResults;
  G4double fDeposit = 0;
  std::uint64_t fTracksOutside = 0;
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

class ReplayRunAction : public G4UserRunAction
{
 public:
  // `source` and `results` are null on worker threads.
  ReplayRunAction(ReplaySource* source, ReplayResults* results) : fSource(source), fResults(results) {}
  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;

 private:
  ReplaySource* fSource;
  ReplayResults* fResults;
  std::chrono::steady_clock::time_point fStart;
};

class ReplayActionInitialization : public G4VUserActionInitialization
{
 public:
  ReplayActionInitialization(ReplaySource& source, ReplayResults& results, const ReplayDetectorConstruction& detector)
    : fSource(source), fResults(results), fDetector(detector)
  {}
  void BuildForMaster() const override;
  void Build() const override;

 private:
  ReplaySource& fSource;
  ReplayResults& fResults;
  const ReplayDetectorConstruction& fDetector;
};

}  // namespace phsp
