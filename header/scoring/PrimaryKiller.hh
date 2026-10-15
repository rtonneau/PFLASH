#ifndef CHEM6_PrimaryKiller_h
#define CHEM6_PrimaryKiller_h 1

#include <G4THitsMap.hh>
#include <G4UImessenger.hh>
#include <G4VPrimitiveScorer.hh>

class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithAnInteger;

/** \file PrimaryKiller.hh*/

// Description:
//   Kill the primary particle:
//   - either after a given energy loss
//   - or after the primary particle has reached a given energy

class PrimaryKiller : public G4VPrimitiveScorer, public G4UImessenger
{
private:
    double fELoss; // cumulated energy loss by the primary

    double fELossRange_Min; // fELoss from which the primary is killed
    double fELossRange_Max; // fELoss from which the event is aborted
    double fKineticE_Min;   // kinetic energy below which the primary is killed
    G4ThreeVector fPhantomSize;
    G4int fVerbose; // verbosity level, G4cout messages are printed only if > 0

    G4UIcmdWithADoubleAndUnit *fpELossUI;
    G4UIcmdWithADoubleAndUnit *fpAbortEventIfELossUpperThan;
    G4UIcmdWithADoubleAndUnit *fpMinKineticE;
    G4UIcmdWith3VectorAndUnit *fpSizeUI;
    G4UIcmdWithAnInteger *fpVerboseUI;

public:
    PrimaryKiller(G4String name, G4int depth = 0);

    virtual ~PrimaryKiller();

    /** Set energy under which the particle should be
     killed*/
    inline void SetEnergyThreshold(double energy) { fKineticE_Min = energy; }

    /** Set the energy loss from which the primary is
     killed*/
    inline void SetMinLossEnergyLimit(double energy) { fELossRange_Min = energy; }

    /** Set the energy loss from which the event is
     aborted*/
    inline void SetMaxLossEnergyLimit(double energy) { fELossRange_Max = energy; }

    /** Set the verbosity level*/
    inline void SetVerboseLevel(G4int level) { fVerbose = level; }

    /** True if a PrimaryKiller killed track 1 in the current event on this
     thread (eLossMin / minKineticE cut, or track 1 leaving the bounding
     box). Per-thread; reset at the start of each event by Initialize().
     Read by TrackingAction to tell a killed primary from a stopped one. */
    static G4bool PrimaryKilledThisEvent();

    /** Method related to G4UImessenger
        used to control energy cuts through macro file
     */
    virtual void SetNewValue(G4UIcommand *command, G4String newValue);

protected:
    virtual G4bool ProcessHits(G4Step *, G4TouchableHistory *);

public:
    virtual void Initialize(G4HCofThisEvent *);
    virtual void EndOfEvent(G4HCofThisEvent *);
    virtual void DrawAll();
    virtual void PrintAll();
};
#endif
