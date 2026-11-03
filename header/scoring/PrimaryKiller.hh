/// \file PrimaryKiller.hh
/// \brief Apply track bounds and primary-electron energy cuts.
/// Requires Geant4 tracking and UI services.
/// Each scorer is used by one thread; the killed-primary flag is thread-local.

#ifndef CHEM6_PrimaryKiller_h
#define CHEM6_PrimaryKiller_h 1

#include <G4THitsMap.hh>
#include <G4UImessenger.hh>
#include <G4VPrimitiveScorer.hh>

class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithAnInteger;
class G4UIcmdWithABool;

/** \file PrimaryKiller.hh*/

/// Bounds-check every track, then apply energy cuts only to primary electrons.
/// Configuration uses Geant4 internal energy/length units; setters do not validate
/// ranges. UI conversion uses Geant4 command parsers.

class PrimaryKiller : public G4VPrimitiveScorer, public G4UImessenger
{
private:
    void PrintAbort(const G4double &energyLoss);
    void PrintKill(const double &kineticEnergy);

    double fELoss; // primary-electron loss in Geant4 internal energy units

    double fELossRange_Min; // fELoss from which the primary is killed
    double fELossRange_Max; // abort when fELoss is strictly above this value
    double fKineticE_Min;   // kill at or below this internal-energy threshold
    G4ThreeVector fPhantomSize;
    G4int fVerbose; // verbosity level, G4cout messages are printed only if > 0
    G4bool fEnergyCuts = true; // false: skip all primary-electron energy cuts

    G4UIcmdWithADoubleAndUnit *fpELossUI;
    G4UIcmdWithADoubleAndUnit *fpAbortEventIfELossUpperThan;
    G4UIcmdWithADoubleAndUnit *fpMinKineticE;
    G4UIcmdWith3VectorAndUnit *fpSizeUI;
    G4UIcmdWithAnInteger *fpVerboseUI;
    G4UIcmdWithABool *fpEnergyCutsUI;

public:
    /// Construct scorer name at copy-number depth and register its UI commands.
    PrimaryKiller(G4String name, G4int depth = 0);

    /// Delete the UI commands.
    virtual ~PrimaryKiller();

    /// Set the primary-electron kinetic-energy threshold, in Geant4 internal units.
    inline void SetEnergyThreshold(double energy) { fKineticE_Min = energy; }

    /// Set the inclusive primary-electron loss threshold, in Geant4 internal units.
    inline void SetMinLossEnergyLimit(double energy) { fELossRange_Min = energy; }

    /// Set the exclusive primary-electron loss threshold for aborting the event.
    /// energy uses Geant4 internal units.
    inline void SetMaxLossEnergyLimit(double energy) { fELossRange_Max = energy; }

    /// Enable or disable all primary-electron energy cuts (loss kill, loss abort,
    /// kinetic-energy kill); the box cut is unaffected.
    inline void SetEnergyCutsEnabled(G4bool enabled) { fEnergyCuts = enabled; }

    /// Print the existing cut diagnostics when level > 0; no range validation.
    inline void SetVerboseLevel(G4int level) { fVerbose = level; }

    /** True if a PrimaryKiller killed track 1 in the current event on this
     thread (eLossMin / minKineticE cut while energy cuts are enabled, or
     track 1 leaving the bounding
     box). Per-thread; reset at the start of each event by Initialize().
     Read by TrackingAction to tell a killed primary from a stopped one. */
    static G4bool PrimaryKilledThisEvent();

    /// Dispatch loss/kinetic-energy/energy-cuts/size/verbosity commands from
    /// newValue; unknown commands are ignored.
    virtual void SetNewValue(G4UIcommand *command, G4String newValue);

protected:
    /// Requires a live tracking step/event; may kill the track or abort the event.
    /// Returns false for box cuts/non-primary/no-loss steps, true after energy-cut handling.
    virtual G4bool ProcessHits(G4Step *, G4TouchableHistory *);

public:
    /// Reset event energy loss and this thread's killed-primary flag; HCE is unused.
    virtual void Initialize(G4HCofThisEvent *);
    /// No operation; the collection argument is unused.
    virtual void EndOfEvent(G4HCofThisEvent *);
    /// No visualization is produced.
    virtual void DrawAll();
    /// No summary is printed.
    virtual void PrintAll();
};
#endif
