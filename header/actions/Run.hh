/// Collects event energy deposits and merges worker scoring results.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Thread-local accumulation; Geant4 serializes worker merges into the master run.

#ifndef CHEM4_Run_h
#define CHEM4_Run_h 1

#include "geometry/DetectorConstruction.hh"
#include "scoring/ReactionCounter.hh"
#include "scoring/PhysicsInteractionCounter.hh"
#include "scoring/MesoSpeciesCounter.hh"
#include "scoring/TrackLengthTable.hh"

#include "G4Run.hh"
#include "globals.hh"

#include <string>

class G4VPrimitiveScorer;
class DetectorConstruction;
/// Collects event energy deposits and merges worker scoring results.
class Run : public G4Run
{
public:
    /// Beam sampled at the first generated event on a worker/serial run
    /// (units carried in the member names, for the dump's manifest).
    struct Beam
    {
        std::string particle;
        double energy_keV = 0.;
        double position_um[3] = {0., 0., 0.};
        double direction[3] = {0., 0., 1.};
    };

    /// Binds live action counters or owned fallbacks; requires initialized mfDetector/Species scoring.
    Run();
    /// Destroy after callbacks and borrowed uses have finished.
    virtual ~Run();

    /// Requires a non-null event and valid Species hit collection. Skips aborted events or missing hit collections.
    /// Sums energy in Geant4 units (MeV), then increments the base run event count.
    virtual void RecordEvent(const G4Event *);
    /// Requires another Run with valid scorers/counters, or this (a no-op).
    /// Merge under Geant4's run-merge lock; no internal locking or type validation.
    virtual void Merge(const G4Run *);

    /// Historical name: returns summed energy deposit in MeV, not dose per mass. No failure path.
    G4double GetSumDose() const { return fEnergyDeposit_MeV; }
    /// Borrowed scorer owned by the sensitive detector; requires valid construction.
    G4VPrimitiveScorer *GetPrimitiveScorer() const { return fScorerRun; }
    /// Borrowed counter for this run/action lifetime; use on its thread or during serialized merge.
    ReactionCounter *GetReactionCounter() const { return fReactionCounter; }
    /// Borrowed process-count counter; same lifetime and threading contract.
    PhysicsInteractionCounter *GetInteractionCounter() const { return fInteractionCounter; }
    /// Borrowed mesoscopic counter (times in ns); same lifetime and threading contract.
    MesoSpeciesCounter *GetMesoSpeciesCounter() const { return fMesoSpeciesCounter; }
    /// Borrowed track table (nm, keV); same lifetime and threading contract.
    TrackLengthTable *GetTrackLengthTable() const { return fTrackLengthTable; }

    /// PrimaryGeneratorAction samples the first event; Merge carries the first available beam to the master.
    /// Copies beam only if not yet captured; caller supplies keV, micrometres and unit direction.
    /// No validation; string allocation exceptions propagate.
    void SetBeamIfUnset(const Beam &beam)
    {
        if (!fHasBeam)
        {
            fBeam = beam;
            fHasBeam = true;
        }
    }
    /// Whether a beam was captured; no failure path.
    G4bool HasBeam() const { return fHasBeam; }
    /// Borrowed beam for this Run lifetime; check HasBeam to distinguish defaults. No failure path.
    const Beam &GetBeam() const { return fBeam; }
    /// Seed of the engine at Run construction; an integer seed, not complete RNG state.
    /// No failure path; worker seeds are not merged into the master seed.
    long GetSeed() const { return fSeed; }

private:
    G4double fEnergyDeposit_MeV;
    G4VPrimitiveScorer *fScorerRun;
    // Points at TimeStepAction's live counter on a thread that runs chemistry
    // (the Serial master, or an MT worker); on the MT master (which never
    // registers a TimeStepAction) falls back to fOwnedReactionCounter, used
    // purely as the Merge() accumulation target. The live counter persists
    // across runs; RunAction::BeginOfRunAction clears it at each run start.
    ReactionCounter *fReactionCounter;
    ReactionCounter fOwnedReactionCounter;
    // Same pattern as fReactionCounter/fOwnedReactionCounter, but for
    // SteppingAction's live PhysicsInteractionCounter.
    PhysicsInteractionCounter *fInteractionCounter;
    PhysicsInteractionCounter fOwnedInteractionCounter;
    // Same pattern again, for TimeStepAction's live MesoSpeciesCounter.
    MesoSpeciesCounter *fMesoSpeciesCounter;
    MesoSpeciesCounter fOwnedMesoSpeciesCounter;
    // Same pattern again, for TrackingAction's live TrackLengthTable (the
    // MT master registers no TrackingAction).
    TrackLengthTable *fTrackLengthTable;
    TrackLengthTable fOwnedTrackLengthTable;

    G4bool fHasBeam = false;
    Beam fBeam;
    long fSeed = 0;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
