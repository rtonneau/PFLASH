/// \file TimeStepAction.hh
/// \brief Definition of the TimeStepAction class
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

class TimeStepAction : public G4UserTimeStepAction
{
public:
  explicit TimeStepAction(const G4VChemistryWorld *chemistryWorld);
  ~TimeStepAction() override;
  TimeStepAction(const TimeStepAction &other) = delete;
  TimeStepAction &operator=(const TimeStepAction &other) = delete;

  void StartProcessing() override;

  void UserPreTimeStepAction() override;
  void UserPostTimeStepAction() override;

  void UserReactionAction(const G4Track &, const G4Track &, const std::vector<G4Track *> *) override;

  void EndProcessing() override;

  ReactionCounter &GetReactionCounter() { return fReactionCounter; }
  /// Species counts at the mesoscopic record times (hand-over to end time,
  /// /chem/meso/timesPerDecade), summed over this thread's events; bulk
  /// species and water excluded. Cleared by RunAction::BeginOfRunAction.
  MesoSpeciesCounter &GetMesoSpeciesCounter() { return fMesoSpeciesCounter; }

private:
  /// Hands the surviving molecules over to the mesoscopic stage and runs it.
  void CompartmentBased();

  /// Merges every live track whose species the scavenger material holds
  /// (e.g. radiolytic O2 while bulk O2 is set) into the bulk pool and kills
  /// it, before the hand-over (Voxelizing then clears all pending
  /// reactions). Voxelizing would
  /// leave such tracks alive, which hangs IRT_syn (see the .cc).
  void MergeScavengerSpeciesIntoBulk(G4double globalTime);

  /// Initial mesh pixel count per side from /chem/meso/voxelSize
  /// (MesoSettings::PixelCount, capped at 65536).
  G4int InitialPixel() const;

  /// Copies the scheduler's record-time counter into fMesoSpeciesCounter,
  /// then resets that counter for the next event.
  void CollectMesoSpecies();

  /// Appends this event's mesoscopic spatial snapshots to the staged
  /// SpeciesMesoSpatial.h5 (only when /chem/meso/spatialOutput is on and
  /// there are records), then clears them. A failure is a JustWarning.
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
  G4double fParticleStageWall = 0.;
};

#endif // ITACTION_H
