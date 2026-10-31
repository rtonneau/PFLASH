/// Resets per-run counters and transfers completed totals to persistent storage.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: One instance per run-manager thread; only the master updates persistent totals.

#ifndef CHEM4_RunAction_h
#define CHEM4_RunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"

#include <chrono>

class G4Run;
class DetectorConstruction;

// Unused legacy declaration; retained with its private member in this pass.
class DoseAccumulable;

/// Resets per-run counters and transfers completed totals to persistent storage.
class RunAction : public G4UserRunAction
{
public:
    /// Creates lifecycle callbacks; GenerateRun supplies the matching Run subtype.
    RunAction();
    /// Destroy after callbacks and borrowed uses have finished.
    virtual ~RunAction();

    /// Allocates a Run for Geant4 ownership; requires initialized scoring/actions.
    virtual G4Run *GenerateRun();
    /// Requires non-null Run. Resets counters before events and resolves reaction bins on the master.
    /// Invalid bin configuration raises FatalException; no bool/error-string result.
    virtual void BeginOfRunAction(const G4Run *);
    /// Requires non-null Run; the master accumulates nonempty totals and wall time in seconds.
    /// Call after worker merges; no files are published here.
    virtual void EndOfRunAction(const G4Run *);

private:
    std::chrono::steady_clock::time_point fRunStart; // master: set in BeginOfRunAction
    DoseAccumulable *fDoseAccumulable = nullptr;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
