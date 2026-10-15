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

/// Run class
///
/// In RecordEvent() there is collected information event per event
/// from Hits Collections, and accumulated statistic for the run
class G4VPrimitiveScorer;
class DetectorConstruction;
class Run : public G4Run
{
public:
    /// The beam as the primary generator had it set when this run started
    /// (units carried in the member names, for the dump's manifest).
    struct Beam
    {
        std::string particle;
        double energy_keV = 0.;
        double position_um[3] = {0., 0., 0.};
        double direction[3] = {0., 0., 1.};
    };

    Run();
    virtual ~Run();

    virtual void RecordEvent(const G4Event *);
    virtual void Merge(const G4Run *);

    G4double GetSumDose() const { return fSumEne; }
    G4VPrimitiveScorer *GetPrimitiveScorer() const { return fScorerRun; }
    ReactionCounter *GetReactionCounter() const { return fReactionCounter; }
    PhysicsInteractionCounter *GetInteractionCounter() const { return fInteractionCounter; }
    MesoSpeciesCounter *GetMesoSpeciesCounter() const { return fMesoSpeciesCounter; }
    TrackLengthTable *GetTrackLengthTable() const { return fTrackLengthTable; }

    // Beam sampled by PrimaryGeneratorAction from its gun on the first event of
    // this run (on a worker thread in MT mode); Merge() carries it to the
    // master run. Only the first call per run has an effect.
    void SetBeamIfUnset(const Beam &beam)
    {
        if (!fHasBeam)
        {
            fBeam = beam;
            fHasBeam = true;
        }
    }
    G4bool HasBeam() const { return fHasBeam; }
    const Beam &GetBeam() const { return fBeam; }
    // Seed of the random engine when this Run was created (on the master:
    // at /run/beamOn, before the run's events consume random numbers).
    long GetSeed() const { return fSeed; }

private:
    G4double fSumEne;
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
