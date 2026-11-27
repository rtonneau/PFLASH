// PhaseSpaceSource: the stage-2 source of phsp-replay. ReplaySource holds the
// replay settings and the global history numbering (one per process, shared
// by every thread); PhaseSpaceSource is the per-thread primary generator that
// injects one Phase-space history per event.
//
// Settings (ReplayMessenger, /phsp/source/...):
//   addFile <path>   PreInit, ordered and repeatable: the global order is file
//                    order, then history index (HistoryCatalog)
//   first <k>        global index of the first history of the next run
//   baseSeed <s>     PreInit: the stage-2 base seed of ADR 0015
// Open() (from ReplayDetectorConstruction::Construct, at /run/initialize)
// opens every file and builds the HistoryCatalog; a file that does not open or
// validate, a file ID given twice, or files with different box sides are a
// FatalException. BeginRun(N) (master BeginOfRunAction, before any event)
// replays global histories first..first+N-1: a range past the end, or one that
// overlaps a range already replayed in this process, is a FatalException.
//
// Event e of a run replays global history first + e (event IDs are assigned by
// the master in order, so the mapping does not depend on the thread count).
// GeneratePrimaries reseeds the engine from HistoryEngineSeeds(baseSeed,
// fileId, historyIndex) (ADR 0015), after G4WorkerRunManager::GenerateEvent
// seeded the event, then adds one primary vertex per record: the record's
// box-frame position (the box is centred at the origin of the stage-2 world),
// its time minus the earliest record time of the history (so the history
// starts at t = 0), and one primary with the record's PDG code, kinetic
// energy, direction and weight. The engine draws nothing before the tracking
// starts, so a history gets the same physical stage in any run, range split or
// thread count.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "HistoryCatalog.hh"

#include "G4VUserEventInformation.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4ParticleDefinition;

namespace phsp
{

// Relative tolerance on box-side comparisons (between files, and against
// /phsp/geom/boxSide): the header stores nm, Geant4 internal units are mm.
inline constexpr double kBoxSideRelativeTolerance = 1e-9;

class ReplaySource
{
 public:
  // PreInit settings.
  void AddFile(const std::filesystem::path& path);
  void SetBaseSeed(std::uint64_t seed) { fBaseSeed = seed; }
  // Optional: the box side the user expects; must equal the files' box side.
  void SetRequestedBoxSide(G4double side) { fRequestedBoxSide = side; }
  // PreInit or Idle: first global history of the next run.
  void SetFirst(std::uint64_t first) { fFirst = first; }

  // Opens the files (once). FatalException on any problem (see above).
  void Open();
  G4bool IsOpen() const { return fCatalog != nullptr; }
  const HistoryCatalog& Catalog() const;
  // Box side of the files (Geant4 length units), after Open().
  G4double BoxSide() const { return fBoxSide; }

  // Master, at the start of a run of `events` events (> 0): checks the range
  // and records it. FatalException on a range past the end or an overlap.
  void BeginRun(std::uint64_t events);
  // Global index of the first history of the current run (read by workers).
  std::uint64_t RunFirst() const { return fRunFirst.load(); }

  const std::vector<std::filesystem::path>& Files() const { return fFiles; }
  std::uint64_t BaseSeed() const { return fBaseSeed; }
  G4bool HasRequestedBoxSide() const { return fRequestedBoxSide > 0; }
  G4double RequestedBoxSide() const { return fRequestedBoxSide; }
  const std::vector<HistoryRange>& Runs() const { return fRuns; }

 private:
  std::vector<std::filesystem::path> fFiles;
  std::uint64_t fBaseSeed = 1;
  std::uint64_t fFirst = 0;
  G4double fRequestedBoxSide = 0;  // 0 = not given
  std::unique_ptr<HistoryCatalog> fCatalog;
  G4double fBoxSide = 0;
  std::atomic<std::uint64_t> fRunFirst{0};
  std::vector<HistoryRange> fRuns;
};

// What an event replays, attached to the G4Event by PhaseSpaceSource.
struct ReplayEventInformation : public G4VUserEventInformation
{
  std::uint64_t globalIndex = 0;
  HistoryKey key;
  HistoryEntry entry;  // the stage-1 history table entry (event ID, box, record count, in-box deposit)
  void Print() const override;
};

// The particle of a PDG code (particle table, then ion table); FatalException if none.
G4ParticleDefinition* ParticleOfPdg(G4int pdg);

class PhaseSpaceSource : public G4VUserPrimaryGeneratorAction
{
 public:
  explicit PhaseSpaceSource(const ReplaySource& source) : fSource(source) {}
  void GeneratePrimaries(G4Event* event) override;

 private:
  const ReplaySource& fSource;
  // MixMaxRng keeps the pointer passed to setTheSeeds: the seeds live here.
  std::array<long, 3> fSeeds = {1, 1, 0};
};

}  // namespace phsp
