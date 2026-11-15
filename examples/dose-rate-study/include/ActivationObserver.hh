// ActivationObserver: the G4UserTimeStepAction of the shared IRT_syn history.
// It watches each source activate, counts it, checks the history, and reads
// the species counts on the record grid at the end.
//
// The sources were captured separately (ticket 02) and pushed by
// SourceInjector into the delayed lists of ONE standalone chemistry run.
// G4Scheduler::Process (Geant4 11.4.3, G4Scheduler.cc) calls StartProcessing,
// then, per delayed list in time order, SynchronizeTracks merges the list into
// the history (MergeNextTimeToMainList; the clock is set to the list's time t
// exactly), calls NewStage (DoProcess) and steps up to the next list's time
// (the last step is clipped to it) or the end time; then EndProcessing. So each
// source activates at a NewStage, and the stage before it ends exactly at t.
//
//   StartProcessing         checks that the scheduler runs IRT_syn; snapshot of
//                           the (empty) history before the first source.
//   UserPostTimeStepAction  after the last step before a merge (clock at the
//                           next list's time, or no track left), snapshot of
//                           the live tracks: ID, configuration, position.
//   NewStage at t           the activation and preservation checks of
//                           ActivationObserverCheck (premature, late, duplicate
//                           or missing activation; an earlier molecule removed
//                           or moved). The molecules that activated are then
//                           added to the molecule counter at t: they were built
//                           with the counter inactive (SourceInjector.hh).
//   UserReactionAction      with a reflecting box, folds a reaction product
//                           that IRT_syn placed outside the box back in
//                           (G4DNAMakeReaction places products between the
//                           reaction site and a random point, which can lie
//                           outside the box).
//   EndProcessing           every source activated; every live track sits at
//                           the end time (and inside the box); the counter at
//                           the end time equals the live molecules per
//                           configuration; species counts on the record grid.
//
// Steady-state stop (doseCutoff mode, EnableSteadyState). At each snapshot
// before a delayed list joins (one per arrival after the first, as no two
// arrivals share a time), the live molecules are counted per species and
// passed to a SteadyStateDetector. When it reports the steady state, the
// history stops there (G4Scheduler::Stop()): the stop time takes the place of
// the end time in the end checks. G4Scheduler still merges the next delayed
// list into the main list after a stop without starting its stage, so the
// molecules of the sources not yet activated may be live at the end: they are
// checked by ActivationObserverCheck::CheckEarlyEnd and left out of the other
// end checks. Only the record times at least kCounterPrecisionNs before the
// stop are measured (SpeciesRows, MeasuredTable). The counter is then switched
// off, as after a failure, before the scheduler deletes the remaining tracks.
//
// Snapshots (doseCutoff mode, EnableSnapshots). At each snapshot time the
// positions and species of every live molecule (water excluded, as in the
// species tables) go to a SnapshotWriter. The scheduler only stops where it is
// told to, so the snapshot times inside a stage are given to it as watched
// times (G4Scheduler::AddWatchedTime): it then ends a step, and runs a
// DoProcess (NewStage), at each of them. Geant4 11.4.3's SynchronizeTracks
// skips the final step to the next delayed list when a watched time lies
// between that list's time and the end time, so the watched times of one
// inter-activation interval are added only once the previous one is over (at
// the last step before the delayed list joins) and always end kSnapshotGuardNs
// before the next list. A stage at a watched time is not an activation: it is
// recognised by its time and skipped by NewStage. A snapshot time within
// kSnapshotGuardNs of an activation, or of the end, is taken at the nearest
// stage boundary (after the activation, before the next list, the final step)
// and carries the clock time of that boundary. Times before the first
// activation (no molecule yet) have no frame. Because the steps are clipped at
// the watched times, a run with snapshots follows a different random stream
// than one without: results agree statistically, not bit for bit.
//
// A failed check never throws through Geant4: the first failure is kept, the
// counter is switched off (so the tracks left are deleted without being
// uncounted) and G4Scheduler::Stop() ends the history; the caller reads
// Failure() after Run().
#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ActivationObserverCheck.hh"
#include "Ledger.hh"
#include "SnapshotWriter.hh"
#include "SourceInjector.hh"
#include "SteadyStateDetector.hh"
#include "TimeGrid.hh"

#include "G4UserTimeStepAction.hh"
#include "globals.hh"

class G4DNABoundingBox;

namespace drs
{

class ActivationObserver : public G4UserTimeStepAction
{
 public:
  // End-of-history checks (valid when Finished()).
  struct EndChecks
  {
    G4int steps = 0;                // G4Scheduler steps of the history
    G4double finalTime = 0;         // scheduler clock at the end (Geant4 units)
    std::size_t liveMolecules = 0;  // live tracks except H2O
    std::size_t liveWater = 0;      // live H2O tracks (the counter ignores H2O)
    std::size_t counterSpecies = 0; // configurations compared with the counter
    G4bool allLiveAtEndTime = false;
    G4bool counterMatchesLive = false;
    G4bool allInsideBox = false;    // true when there is no box
    G4bool stoppedEarly = false;    // steady-state stop
    G4double checkTime = 0;         // the end time, or the stop time (Geant4 units)
    std::size_t pendingSourceMolecules = 0;  // live, never activated (steady-state stop only)
  };

  // sources: from SourceInjector (contiguous IDs from 1); recordTimesNs:
  // increasing, the last one is the end time; box: nullptr = unbounded;
  // counterId: the G4MoleculeCounter registered for the history.
  // Throws std::invalid_argument on inconsistent sources.
  void Configure(const std::vector<InjectedSource>& sources, const std::vector<double>& recordTimesNs,
                 const G4DNABoundingBox* box, G4int counterId);

  // Wall seconds between HISTORY_PROGRESS console lines (simulated time,
  // sources activated, live molecules, steps); 0 (default) = none. Lines are
  // checked once per scheduler step, so one long step delays the next line.
  void SetProgressInterval(double seconds) { fProgressIntervalS = seconds; }

  // Turns the steady-state stop on (after Configure). Throws
  // std::invalid_argument on invalid settings.
  void EnableSteadyState(const SteadyStateSettings& settings);

  // Turns the snapshots on (after Configure, once the chemistry is
  // initialized: the species table is read now): creates the file and keeps
  // the snapshot times, increasing, in ns. Throws std::runtime_error when the
  // file cannot be written (or HDF5 is missing) and std::invalid_argument on
  // invalid times.
  void EnableSnapshots(const std::filesystem::path& file, const std::vector<double>& timesNs);

  void StartProcessing() override;
  void NewStage() override;
  void UserPostTimeStepAction() override;
  void UserReactionAction(const G4Track& trackA, const G4Track& trackB,
                          const std::vector<G4Track*>* products) override;
  void EndProcessing() override;

  // Empty when every check passed.
  const std::string& Failure() const { return fFailure; }
  // Non-empty when the scheduler does not run IRT_syn (Failure() is then set too).
  const std::string& ModelProblem() const { return fModelProblem; }
  G4bool Finished() const { return fFinished; }

  // Stages and observed activation times (after Configure).
  const ActivationObserverCheck& Activations() const { return fCheck.value(); }
  const EndChecks& End() const { return fEnd; }
  // timeNs, species, count (the history and seed columns are left to the caller).
  const std::vector<SpeciesRow>& SpeciesRows() const { return fRows; }
  // The same measured counts, one vector per record time in species order
  // (every species of the chemistry, water excluded, sorted by name).
  const SpeciesTable& MeasuredTable() const { return fTable; }
  // Steady-state stop: nullptr when not enabled (or before the history ran).
  const SteadyStateDetector* SteadyState() const { return fSteadyState ? &*fSteadyState : nullptr; }
  G4bool StoppedEarly() const { return fEnd.stoppedEarly; }
  std::size_t ProductFoldCount() const { return fProductFolds; }
  // Snapshots: frame times are the clock times of the
  // frames written, in order; the requested times are those of EnableSnapshots.
  G4bool SnapshotsEnabled() const { return fSnapshotWriter != nullptr; }
  const std::vector<double>& SnapshotFrameTimesNs() const { return fFrameTimesNs; }
  std::size_t SnapshotTimesRequested() const { return fSnapshotTimesNs.size(); }
  std::size_t SnapshotTimesDropped() const { return fSnapshotsDropped; }
  // Requested times that shared the frame of another time taken in the same call.
  std::size_t SnapshotTimesMerged() const { return fSnapshotsMerged; }
  std::size_t SnapshotMoleculeTotal() const { return fSnapshotMolecules; }
  // Flushes and closes the snapshot file (end of the run; also on failure).
  void CloseSnapshots();
  // For a history with no source (chemistry not run): zero rows on the record grid.
  void FinishEmpty();

 private:
  void Fail(const std::string& message);
  void TakeSnapshot(const std::vector<const G4Track*>& live);
  void SampleSteadyState(const std::vector<const G4Track*>& live, G4double now);
  void Finish();
  void ReportProgress(G4bool force);
  void ArmNextInterval(G4double endTime);
  void CaptureDue(double limitNs, bool strict, double clockNs, const std::vector<const G4Track*>& live);
  void CaptureFrame(double labelNs, const std::vector<const G4Track*>& live);

  std::optional<ActivationObserverCheck> fCheck;
  double fProgressIntervalS = 0;
  std::chrono::steady_clock::time_point fProgressStart;
  std::chrono::steady_clock::time_point fLastProgress;
  std::vector<double> fRecordTimesNs;
  const G4DNABoundingBox* fBox = nullptr;
  G4int fCounterId = -1;
  std::optional<SteadyStateSettings> fSteadySettings;
  std::optional<SteadyStateDetector> fSteadyState;
  std::map<std::string, std::size_t> fSpeciesIndex;  // steady-state sample order
  // Snapshots.
  std::unique_ptr<SnapshotWriter> fSnapshotWriter;
  std::vector<double> fSnapshotTimesNs;       // requested, increasing
  std::vector<double> fActivationTimesNs;     // distinct activation times, increasing
  std::map<std::string, std::uint8_t> fSnapshotSpecies;
  std::vector<double> fFrameTimesNs;          // clock time of each frame written
  std::set<double> fWatched;                  // watched times added, in Geant4 units
  std::size_t fNextFrame = 0;                 // first requested time not yet captured
  std::size_t fNextArm = 0;                   // first requested time not yet considered for arming
  std::size_t fArmedIntervals = 0;
  std::size_t fSnapshotsDropped = 0;
  std::size_t fSnapshotsMerged = 0;
  std::size_t fSnapshotMolecules = 0;
  G4bool fStopRequested = false;
  G4double fStopTime = 0;

  EndChecks fEnd;
  std::vector<SpeciesRow> fRows;
  SpeciesTable fTable;
  std::size_t fProductFolds = 0;
  std::string fFailure;
  std::string fModelProblem;
  G4bool fFinished = false;
};

}  // namespace drs
