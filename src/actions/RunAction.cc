/// \file RunAction.cc
/// \brief Implementation of the RunAction class

#include "actions/RunAction.hh"

#include "geometry/DetectorConstruction.hh"
#include "chemistry/DnaChemistryList.hh"
#include "core/DnaLogger.hh"
#include "physics/PhysicsList.hh"
#include "actions/PrimaryGeneratorAction.hh"
#include "actions/Run.hh"
#include "scoring/RunAccumulator.hh"
#include "scoring/RunManifest.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction() : G4UserRunAction()
{
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4Run *RunAction::GenerateRun()
{
    Run *run = new Run();
    return run;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run *run)
{
    if (IsMaster())
        fRunStart = std::chrono::steady_clock::now();

    // ensure that the chemistry is notified!
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
        G4DNAChemistryManager::GetInstanceIfExists()->BeginOfRunAction(run);

    // Give the reaction/interaction/meso-species counters and the track-length
    // table a per-run lifetime. On a thread that owns the
    // TimeStepAction/SteppingAction/TrackingAction (Serial master, MT worker)
    // the new Run points at their live, persistent counters; on the MT master
    // it points at its own, already empty ones. This is the only reset.
    auto *thisRun = static_cast<const Run *>(run);
    thisRun->GetReactionCounter()->Clear();
    thisRun->GetInteractionCounter()->Clear();
    thisRun->GetMesoSpeciesCounter()->Clear();
    thisRun->GetTrackLengthTable()->Clear();

    if (IsMaster())
    {
        G4cout << "### Run " << run->GetRunID() << " starts." << G4endl;

        // Resolve any /chem/reaction/timeBinsFixed or timeBinsList macro
        // command into ReactionCounter's shared bin-edge table. Must happen
        // here (not earlier) since "fixed" mode depends on the chemistry
        // scheduler's end time, which a macro may still override between
        // /run/initialize and /run/beamOn.
        auto *physicsList = dynamic_cast<const PhysicsList *>(
            G4RunManager::GetRunManager()->GetUserPhysicsList());
        if (physicsList != nullptr)
            physicsList->GetChemistryList()->ApplyReactionTimeBinning();
    }

    // informs the runManager to save random number seed
    G4RunManager::GetRunManager()->SetRandomNumberStore(false);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run *run)
{
    // ensure that the chemistry is notified!
    if (G4DNAChemistryManager::GetInstanceIfExists() != nullptr)
        G4DNAChemistryManager::GetInstanceIfExists()->EndOfRunAction(run);

    G4int nofEvents = run->GetNumberOfEvent();
    if (nofEvents == 0)
        return;

    if (IsMaster())
    {
        auto *masterRun = static_cast<const Run *>(run);

        // Species yields keep accumulating on their own (the ScoreSpecies
        // scorer is itself the persistent, SD-registered accumulator, fed by
        // Run::Merge -> AbsorbResultsFromWorkerScorer). Energy deposit and
        // the two counters don't have that luxury -- Run is recreated fresh
        // every beamOn -- so fold this run's totals into RunAccumulator's
        // persistent, cross-run storage instead. Nothing is written to disk
        // here: issue /run/dumpDataAndReset (or let the exit-time safety net
        // in sim.cc fire) to flush everything.
        RunAccumulator::Accumulate(masterRun->GetSumDose(), nofEvents,
                                    *masterRun->GetReactionCounter(),
                                    *masterRun->GetInteractionCounter(),
                                    *masterRun->GetMesoSpeciesCounter(),
                                    *masterRun->GetTrackLengthTable());

        // One entry per run for the dump's manifest (beam, events, seed,
        // this run's energy deposit, Begin->EndOfRunAction wall time).
        RunManifest::RecordRun(
            *masterRun,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - fRunStart).count());

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] accumulated this run's energy/reaction/interaction data -- "
                          "use /run/dumpDataAndReset to write everything to files");
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
