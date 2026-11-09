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
// A failed check never throws through Geant4: the first failure is kept, the
// counter is switched off (so the tracks left are deleted without being
// uncounted) and G4Scheduler::Stop() ends the history; the caller reads
// Failure() after Run().
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ActivationObserverCheck.hh"
#include "Ledger.hh"
#include "SourceInjector.hh"

#include "G4UserTimeStepAction.hh"
#include "globals.hh"

class G4DNABoundingBox;

namespace ptirt
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
  };

  // sources: from SourceInjector (contiguous IDs from 1); recordTimesNs:
  // increasing, the last one is the end time; box: nullptr = unbounded;
  // counterId: the G4MoleculeCounter registered for the history.
  // Throws std::invalid_argument on inconsistent sources.
  void Configure(const std::vector<InjectedSource>& sources, const std::vector<double>& recordTimesNs,
                 const G4DNABoundingBox* box, G4int counterId);

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
  std::size_t ProductFoldCount() const { return fProductFolds; }
  // For a history with no source (chemistry not run): zero rows on the record grid.
  void FinishEmpty();

 private:
  void Fail(const std::string& message);
  void TakeSnapshot();
  void Finish();

  std::optional<ActivationObserverCheck> fCheck;
  std::vector<double> fRecordTimesNs;
  const G4DNABoundingBox* fBox = nullptr;
  G4int fCounterId = -1;

  EndChecks fEnd;
  std::vector<SpeciesRow> fRows;
  std::size_t fProductFolds = 0;
  std::string fFailure;
  std::string fModelProblem;
  G4bool fFinished = false;
};

}  // namespace ptirt
