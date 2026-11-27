// RecordActions: the user actions of phsp-record (stage 1).
//
// Recording rule (ADR 0014): every Inward crossing of any track into a
// scoring box is one record, and nothing is killed. A step whose pre-step
// point has status fGeomBoundary and lies in a scoring box has just entered
// that box (a box has no daughters and is the only daughter of its DNA shell,
// so the track came from the shell); the record is that pre-step point: PDG
// code, track ID, kinetic energy, position, direction, global time and weight,
// as in the Geant4 example dna/phasespace (SteppingAction.cc). The position is
// moved into the frame of the box entered with the translation of the touched
// box (whose index is the copy number of its shell, checked against
// RecordDetectorConstruction::BoxCentre), not by folding the global position,
// which is ambiguous on a face.
//
// One Phase-space history per (primary, box): the records and the in-box
// deposit (energy deposited in steps whose pre-step point is in the box) of an
// event are kept per box, and each box with at least one record gives its own
// history, in its own frame. Stage 2 replays one history per event, so
// crossings of distinct boxes are never superposed in one box.
//
// MT: each worker thread fills its own RecordRun. G4WorkerRunManager merges
// every worker run into the master run (RecordRun::Merge, called under the
// run manager's merge mutex) at the end of the run; the master RecordRunAction
// then sorts the histories by HistoryOrderKey (stage-1 event ID, box iy, box ix) and
// hands them to RecordOutcome. Event IDs and per-event seeds are assigned by
// the master in event order, so the merged, sorted list does not depend on the
// thread count.
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "PhaseSpaceWriter.hh"

#include "G4Run.hh"
#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

class G4GeneralParticleSource;

namespace phsp
{

class RecordDetectorConstruction;

// What the master's run produced, read by phsp-record's main() after the macro.
struct RecordOutcome
{
  G4int runsWithEvents = 0;
  std::uint64_t primariesFired = 0;
  std::vector<RecordedHistory> histories;  // sorted by HistoryOrderKey
  // (Event, box) pairs with an in-box deposit but no record (a primary
  // started inside a box): not written, reported as a warning. The total is
  // summed in (event ID, copy number) order, so it does not depend on the
  // thread count (the sidecar records it).
  std::uint64_t depositWithoutRecordEvents = 0;
  G4double depositWithoutRecordEv = 0;
  G4double wallTimeS = 0;  // master BeginOfRunAction to EndOfRunAction
  G4int threads = 0;
};

// One (event, box) pair that deposited energy in a box without an inward
// crossing. Kept as a list and summed by the master after sorting, so the
// floating-point total does not depend on the thread count.
struct DepositWithoutRecord
{
  std::uint64_t stage1EventId = 0;
  G4int copy = 0;
  G4double depositEv = 0;
};

// Records and in-box deposit of the current event in one box.
struct RecordBoxBuffer
{
  std::vector<PhaseSpaceRecord> records;
  G4double deposit = 0;  // Geant4 energy units
};

// Per-thread buffer of the current event, by box copy number (= box index,
// whose order is the (iy, ix) order of HistoryOrderKey).
using RecordEventBuffer = std::map<G4int, RecordBoxBuffer>;

class RecordRun : public G4Run
{
 public:
  void AddHistory(RecordedHistory history) { fHistories.push_back(std::move(history)); }
  void AddDepositWithoutRecord(std::uint64_t stage1EventId, G4int copy, G4double depositEv);
  // Appends the worker run's histories (moved out of it: the worker run is
  // deleted after the merge) and its counters.
  void Merge(const G4Run* run) override;

  std::vector<RecordedHistory>& Histories() { return fHistories; }
  std::vector<DepositWithoutRecord>& DepositsWithoutRecord() { return fDepositsWithoutRecord; }

 private:
  mutable std::vector<RecordedHistory> fHistories;  // mutable: Merge moves from the worker run
  mutable std::vector<DepositWithoutRecord> fDepositsWithoutRecord;
};

class RecordPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
 public:
  RecordPrimaryGeneratorAction();
  ~RecordPrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event* event) override;

 private:
  std::unique_ptr<G4GeneralParticleSource> fSource;  // its messenger provides /gps/...
};

class RecordSteppingAction : public G4UserSteppingAction
{
 public:
  RecordSteppingAction(const RecordDetectorConstruction& detector, RecordEventBuffer& buffer)
    : fDetector(detector), fBuffer(buffer)
  {}
  void UserSteppingAction(const G4Step* step) override;

 private:
  const RecordDetectorConstruction& fDetector;
  RecordEventBuffer& fBuffer;
};

class RecordEventAction : public G4UserEventAction
{
 public:
  explicit RecordEventAction(const RecordDetectorConstruction& detector) : fDetector(detector) {}
  void BeginOfEventAction(const G4Event* event) override;
  void EndOfEventAction(const G4Event* event) override;
  RecordEventBuffer& Buffer() { return fBuffer; }

 private:
  const RecordDetectorConstruction& fDetector;
  RecordEventBuffer fBuffer;
};

class RecordRunAction : public G4UserRunAction
{
 public:
  // `outcome` is null on worker threads.
  explicit RecordRunAction(RecordOutcome* outcome) : fOutcome(outcome) {}
  G4Run* GenerateRun() override { return new RecordRun(); }
  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;

 private:
  RecordOutcome* fOutcome;
  std::chrono::steady_clock::time_point fStart;
};

class RecordActionInitialization : public G4VUserActionInitialization
{
 public:
  RecordActionInitialization(const RecordDetectorConstruction& detector, RecordOutcome& outcome)
    : fDetector(detector), fOutcome(outcome)
  {}
  void BuildForMaster() const override;
  void Build() const override;

 private:
  const RecordDetectorConstruction& fDetector;
  RecordOutcome& fOutcome;
};

}  // namespace phsp
