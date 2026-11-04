/// \file TimeStepAction.hh
/// Runs particle-based chemistry and the mesoscopic hand-over.
/// Dependencies: Geant4 and project scoring services; not standard-library-only.
/// Thread safety: One action per worker/serial thread; scheduler and mesh-action alias are thread-local.
///
/// Per-thread chemistry time-step action. The particle-based stage (IRT_syn)
/// runs up to the hand-over time; at the first time step reaching it, the
/// remaining molecules are handed to a G4DNAEventScheduler (compartment-based
/// mesoscopic stage, Gillespie on a cell mesh) that runs to the end time.
/// Pattern: Geant4 UHDR example, TimeStepAction::CompartmentBased.
/// With the mesoscopic stage off (/chem/meso/enable false, or the SBS model,
/// MesoSettings::StageEnabled) there is no hand-over: the particle-based stage
/// runs to the end time and the event scheduler is unused. Under SBS it also
/// adds the scheduler's user minimum time steps (chem1-chem6 pattern).

#ifndef ITACTION_H
#define ITACTION_H

#include "G4Timer.hh"
#include "G4UserTimeStepAction.hh"
#include "scoring/MesoSpeciesCounter.hh"
#include "scoring/ReactionCounter.hh"

#include <memory>

class G4DNAEventScheduler;
class G4VChemistryWorld;

/// Per-thread stage lifecycle and reaction/species accounting; not shared between workers.
class TimeStepAction : public G4UserTimeStepAction
{
public:
  /// Borrows chemistryWorld, which must outlive the action; null raises FatalException.
  /// Creates this thread's mesoscopic scheduler even when the stage is disabled.
  explicit TimeStepAction(const G4VChemistryWorld *chemistryWorld);
  /// Releases the scheduler and clears this thread's mesh-action alias.
  ~TimeStepAction() override;
  /// Noncopyable: scheduler ownership and the thread-local alias belong to one action.
  TimeStepAction(const TimeStepAction &other) = delete;
  /// Assignment is disabled for the same ownership reason.
  TimeStepAction &operator=(const TimeStepAction &other) = delete;

  /// Starts event chemistry after configuration; installs SBS steps or mesoscopic record times.
  /// Requires a configured thread scheduler/world; invalid log-grid input throws invalid_argument.
  void StartProcessing() override;

  /// Feeds the scheduler's particle-based counter when mesoscopic processing is enabled.
  /// Geant4 calls this between StartProcessing and EndProcessing.
  void UserPreTimeStepAction() override;
  /// Hands over once when global time reaches the configured threshold (Geant4 time units).
  /// Requires initialized event state; Geant4 scheduler exceptions propagate.
  void UserPostTimeStepAction() override;

  /// Counts a reaction between the two molecular tracks at scheduler time; products are unused.
  /// Requires a frozen reaction table. Missing pair logs a warning and returns; invalid cached
  /// reaction IDs can throw out_of_range. Called in the particle-based stage.
  void UserReactionAction(const G4Track &, const G4Track &, const std::vector<G4Track *> *) override;

  /// Stops timing, restores molecule-counter state and collects mesoscopic counts.
  /// Called after event chemistry; scheduler/allocation exceptions propagate.
  void EndProcessing() override;

  /// Borrowed per-thread counter for this action's lifetime; caller controls clearing.
  /// No failure path; do not concurrently mutate it from another thread.
  ReactionCounter &GetReactionCounter() { return fReactionCounter; }
  /// Species counts at the mesoscopic record times (hand-over to end time,
  /// /chem/meso/timesPerDecade), summed over this thread's events; bulk
  /// species and water excluded; times are ns. Borrowed for this action's lifetime,
  /// no failure path. Cleared by RunAction::BeginOfRunAction; owning-thread access only.
  MesoSpeciesCounter &GetMesoSpeciesCounter() { return fMesoSpeciesCounter; }

private:
  /// Hands the surviving molecules over to the mesoscopic stage and runs it.
  void CompartmentBased();

  /// Debug-only count/log before scavenger merging; time uses Geant4 units.
  void CountAndReportParticleTracks(G4double globalTime, G4int pixel,
                                    long long &handOverTotal) const;
  /// Debug-only conservation check immediately after mesh initialization.
  void ReportInitializedMesh(long long handOverTotal);

  /// Merges every live track whose species the scavenger material holds
  /// (e.g. radiolytic O2 while bulk O2 is set) into the bulk pool and kills
  /// it, before the hand-over (Voxelizing then clears all pending
  /// reactions). Voxelizing would
  /// leave such tracks alive, which hangs IRT_syn (see the .cc).
  void MergeScavengerSpeciesIntoBulk(G4double globalTime);

  /// Initial mesh pixel count per side from /chem/meso/voxelSize
  /// (MesoSettings::PixelCount, capped at 65536).
  G4int InitialPixel() const;

  /// Copies the mesh action's recorded counts into fMesoSpeciesCounter,
  /// then resets the separate scheduler counter for the next event.
  void CollectMesoSpecies();

  /// Writes this event's mesoscopic spatial snapshots in the formats chosen for
  /// SpeciesMesoSpatial (h5: appended to the staged file; root: rows added to
  /// the staged table), when there are records, then clears them. A failure is
  /// a JustWarning.
  void WriteSpatialSnapshots();

  ReactionCounter fReactionCounter;
  MesoSpeciesCounter fMesoSpeciesCounter;
  const G4VChemistryWorld *fpChemWorld = nullptr;
  std::unique_ptr<G4DNAEventScheduler> fpEventScheduler;
  /// True once this event's chemistry has been handed over.
  G4bool fHandedOver = false;
  /// Whether this event's chemistry has a mesoscopic stage (set in
  /// StartProcessing from MesoSettings::StageEnabled).
  G4bool fMesoOn = true;
  /// True once the SBS user time steps were given to this thread's scheduler.
  G4bool fSbsTimeStepsAdded = false;
  /// Debug only: true when the hand-over changed the molecule total.
  G4bool fHandOverDrift = false;
  /// The molecule counter is muted from the hand-over to EndProcessing;
  /// these restore its previous state.
  G4bool fCounterMuted = false;
  G4bool fCounterWasActive = true;
  /// Per-event chemistry wall time, split at the hand-over.
  G4Timer fChemTimer;
  G4double fParticleStageWall_s = 0.;
};

#endif // ITACTION_H
