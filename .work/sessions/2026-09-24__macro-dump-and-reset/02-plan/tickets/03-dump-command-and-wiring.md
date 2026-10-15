# Ticket 03: dump-command-and-wiring

**Acceptance Criteria:**
- [ ] `/run/dumpDataAndReset [prefix]` (Idle-state UI command) writes `Species.Txt`/CSVs (via a direct `ScoreSpecies` lookup + `ASCII()`/`OutputAndClear()`), `Reactions.Txt`/CSV/`ReactionsMetadata.csv`, `EnergyDeposit.Txt`, and `PhysicsInteractions.Txt`/CSV for everything accumulated since the last dump (or program start), every filename prefixed literally by `prefix` (default `""`), then resets all counters.
- [ ] Reusing a `prefix` already used earlier in the same process raises a fatal `G4Exception` and writes nothing.
- [ ] `RunAction::EndOfRunAction` no longer writes any files directly -- it only calls `RunAccumulator::Accumulate(...)` once per run (species yields keep accumulating automatically, unchanged).
- [ ] If a macro never issues `/run/dumpDataAndReset` and data is still pending when `sim.exe` is about to exit, a safety-net flush fires automatically with prefix `EndOfRun_`, bypassing the uniqueness check.
- [ ] `CLAUDE.md`, `.claude/.claude-project.json`, and `.claude/geant4-instructions.md` describe the new behavior accurately (no stale references to per-run auto-write/clear or to the now-obsolete `_bis` renaming caveat for these files).
- [ ] Manual verification (see Verification Step) confirms: two `beamOn`s + an explicit `/run/dumpDataAndReset run1_` produce `run1_`-prefixed files with the combined data of both runs; a subsequent `beamOn` with no further dump command still produces `EndOfRun_`-prefixed files at exit; reusing `run1_` a second time aborts with a fatal exception.

**Files to Touch:**
- Create: `header/RunAccumulatorMessenger.hh`
- Create: `src/RunAccumulatorMessenger.cc`
- Modify: `src/RunAction.cc`
- Modify: `sim.cc`
- Modify: `CLAUDE.md`
- Modify: `.claude/.claude-project.json`
- Modify: `.claude/geant4-instructions.md`

**Verification Step:**

Run:
```bash
cmake --build build --target sim
cd build
cat > macro/scratch_dump_reset.in << 'EOF'
/run/initialize
/gun/particle e-
/gun/energy 25 keV
/run/beamOn 2
/run/beamOn 2
/run/dumpDataAndReset run1_
/run/beamOn 2
EOF
./sim scratch_dump_reset.in > run.log 2>&1
grep -c "RunAccumulatorMessenger" run.log
ls run1_Species.Txt run1_Reactions.Txt run1_EnergyDeposit.Txt run1_PhysicsInteractions.Txt
ls EndOfRun_Species.Txt EndOfRun_Reactions.Txt EndOfRun_EnergyDeposit.Txt EndOfRun_PhysicsInteractions.Txt
```

Expected:
- `run.log` contains `[RunAccumulatorMessenger] dumped and reset (prefix='run1_')` once and `[RunAccumulatorMessenger] dumped and reset (prefix='EndOfRun_')` once (2 total).
- Both sets of prefixed files exist and are non-empty; `run1_Species.Txt` reflects 4 events (2+2 beamOn), `EndOfRun_Species.Txt` reflects 2 events.
- Exit code 0, no `FatalException` in the log.

Then verify the refusal path:
```bash
cat > macro/scratch_dump_reset_dupe.in << 'EOF'
/run/initialize
/gun/particle e-
/gun/energy 25 keV
/run/beamOn 1
/run/dumpDataAndReset dupe_
/run/beamOn 1
/run/dumpDataAndReset dupe_
EOF
./sim scratch_dump_reset_dupe.in > run2.log 2>&1; echo "exit=$?"
grep -c "FatalException" run2.log
```
Expected: non-zero exit code, `run2.log` contains a `FatalException` /
`G4Exception` mentioning `dupe_` was already used.

Delete the scratch macros and generated output files afterward (they
are untracked scratch, not part of the repo).

**Notes:**

This is the integration ticket -- no unit test, matching this project's
existing convention that messengers (`DnaLoggerMessenger`,
`OutputDirMessenger`) are never unit-tested; `RunAccumulatorMessenger`
follows the same pattern. Everything it touches (`G4SDManager`,
`ScoreSpecies`, `G4AnalysisManager`) requires a live Geant4 kernel.

**Step 1 — write `header/RunAccumulatorMessenger.hh`:**

```cpp
#ifndef RUN_ACCUMULATOR_MESSENGER_HH
#define RUN_ACCUMULATOR_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file RunAccumulatorMessenger.hh
    UI command to flush the accumulated species/reaction/interaction/energy
    data to disk and reset the underlying counters, from a macro file:
    /run/dumpDataAndReset [prefix]

    Idle-state only (issue it between /run/beamOn calls). prefix (optional,
    default "") is prepended literally (no separator inserted) to every
    output filename written by this flush. Reusing a prefix already used
    earlier in this process is a fatal error -- see
    RunAccumulator::TryReservePrefix.

    Also exposes FlushIfPending(), called once from sim.cc right before the
    run manager is destroyed, as a safety net so data accumulated but never
    manually flushed isn't silently lost.
*/
class RunAccumulatorMessenger : public G4UImessenger
{
public:
    RunAccumulatorMessenger();
    virtual ~RunAccumulatorMessenger();

    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    virtual G4String GetCurrentValue(G4UIcommand *command);

    /// If any data is pending, flushes it under autoPrefix (bypassing the
    /// prefix-uniqueness check -- this path must never fail). No-op
    /// otherwise. Safe to call even if /run/dumpDataAndReset was never
    /// issued.
    void FlushIfPending(const G4String &autoPrefix);

private:
    G4bool DumpAndReset(const G4String &prefix, G4bool enforceUniqueness, G4String &err);

    G4UIcmdWithAString *fpDumpCmd;
};

#endif // RUN_ACCUMULATOR_MESSENGER_HH
```

**Step 2 — write `src/RunAccumulatorMessenger.cc`:**

```cpp
#include "RunAccumulatorMessenger.hh"
#include "RunAccumulator.hh"
#include "OutputDir.hh"
#include "ScoreSpecies.hh"
#include "DnaLogger.hh"

#include "G4UIcmdWithAString.hh"
#include "G4SDManager.hh"
#include "G4MultiFunctionalDetector.hh"
#include "G4AnalysisManager.hh"
#include "G4UnitsTable.hh"

#include <fstream>

RunAccumulatorMessenger::RunAccumulatorMessenger()
    : G4UImessenger()
{
    fpDumpCmd = new G4UIcmdWithAString("/run/dumpDataAndReset", this);
    fpDumpCmd->SetGuidance(
        "Write Species/Reactions/EnergyDeposit/PhysicsInteractions output "
        "files for everything accumulated since the last dump (or program "
        "start), then reset all counters. Optional prefix is prepended "
        "literally to every output filename (no separator inserted). Fatal "
        "if prefix was already used earlier in this run.");
    fpDumpCmd->SetParameterName("prefix", /*omittable=*/true);
    fpDumpCmd->SetDefaultValue("");
    fpDumpCmd->AvailableForStates(G4State_Idle);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

RunAccumulatorMessenger::~RunAccumulatorMessenger()
{
    delete fpDumpCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    if (command == fpDumpCmd)
    {
        G4String err;
        if (!DumpAndReset(newValue, /*enforceUniqueness=*/true, err))
        {
            G4Exception("RunAccumulatorMessenger::SetNewValue", "DuplicateDumpPrefix",
                        FatalException, err.c_str());
        }
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4String RunAccumulatorMessenger::GetCurrentValue(G4UIcommand * /*command*/)
{
    return "";
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

void RunAccumulatorMessenger::FlushIfPending(const G4String &autoPrefix)
{
    if (!RunAccumulator::HasPendingData())
        return;

    G4String err;
    DumpAndReset(autoPrefix, /*enforceUniqueness=*/false, err);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....

G4bool RunAccumulatorMessenger::DumpAndReset(const G4String &prefix, G4bool enforceUniqueness,
                                              G4String &err)
{
    if (!RunAccumulator::TryReservePrefix(prefix, enforceUniqueness, err))
        return false;

    OutputDir::SetPrefix(prefix);

    // Species: the ScoreSpecies scorer is itself the persistent,
    // SD-registered accumulator (unlike energy/reactions/interactions,
    // which RunAccumulator tracks separately) -- same lookup Run::Run()
    // uses.
    auto *mfdet = dynamic_cast<G4MultiFunctionalDetector *>(
        G4SDManager::GetSDMpointer()->FindSensitiveDetector("mfDetector"));
    if (mfdet != nullptr)
    {
        G4int collectionId = G4SDManager::GetSDMpointer()->GetCollectionID("mfDetector/Species");
        auto *scorer = dynamic_cast<ScoreSpecies *>(mfdet->GetPrimitive(collectionId));
        if (scorer != nullptr)
        {
            scorer->ASCII();          // Species.Txt (human-readable)
            scorer->OutputAndClear(); // Species_nt_species(_all).csv, then clears the scorer
        }
    }

    // Reactions
    const ReactionCounter &reactionCounter = RunAccumulator::GetAccumulatedReactionCounter();
    std::ofstream reactionsOut(OutputDir::Resolve("Reactions.Txt"));
    reactionCounter.WriteAscii(reactionsOut);
    reactionsOut.close();

    G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetDefaultFileType("csv");
    reactionCounter.WriteCsv(analysisManager);

    std::ofstream metadataOut(OutputDir::Resolve("ReactionsMetadata.csv"));
    reactionCounter.WriteMetadata(metadataOut);
    metadataOut.close();

    // Energy
    std::ofstream energyOut(OutputDir::Resolve("EnergyDeposit.Txt"));
    energyOut << "Total energy deposited in simulation volume: "
              << G4BestUnit(RunAccumulator::GetAccumulatedEnergy(), "Energy") << "\n";
    energyOut.close();

    // Physics interactions
    const PhysicsInteractionCounter &interactionCounter =
        RunAccumulator::GetAccumulatedInteractionCounter();
    std::ofstream interactionsOut(OutputDir::Resolve("PhysicsInteractions.Txt"));
    interactionCounter.WriteAscii(interactionsOut);
    interactionsOut.close();

    std::ofstream interactionsCsv(OutputDir::Resolve("PhysicsInteractions.csv"));
    interactionCounter.WriteCsv(interactionsCsv);
    interactionsCsv.close();

    RunAccumulator::ClearAccumulated();
    OutputDir::SetPrefix("");

    // Plain G4cout (not DnaLogger): this is the run.successMarkers line the
    // project's own smoke-test verification checks for -- see
    // .claude/.claude-project.json and .claude/geant4-instructions.md.
    G4cout << "[RunAccumulatorMessenger] dumped and reset (prefix='" << prefix << "')" << G4endl;

    return true;
}
```

**Step 3 — trim `src/RunAction.cc`.** Replace the include block at the
top of the file:

old:
```cpp
#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "DnaChemistryList.hh"
#include "DnaLogger.hh"
#include "OutputDir.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "ReactionCounter.hh"
#include "Run.hh"
#include "ScoreSpecies.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

#include <fstream>
```

new:
```cpp
#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "DnaChemistryList.hh"
#include "DnaLogger.hh"
#include "PhysicsList.hh"
#include "PrimaryGeneratorAction.hh"
#include "Run.hh"
#include "RunAccumulator.hh"

#include "G4DNAChemistryManager.hh"

#include "G4AccumulableManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
```

Then replace the entire `if (IsMaster())` block inside
`RunAction::EndOfRunAction`:

old:
```cpp
    if (IsMaster())
    {
        auto *masterRun = static_cast<const Run *>(run);

        // Write the radiolytic-species yields (merged across worker threads by
        // Run::Merge -> ScoreSpecies::AbsorbResultsFromWorkerScorer).
        auto *scorer = dynamic_cast<ScoreSpecies *>(masterRun->GetPrimitiveScorer());
        if (scorer != nullptr)
        {
            const G4int recorded = scorer->GetNumberOfRecordedEvents();
            scorer->ASCII();          // Species.Txt (human-readable)
            scorer->OutputAndClear(); // Species_nt_species(_all).csv, then clears the scorer
            G4cout << "[RunAction] species yields written (Species.Txt / Species_nt_species*.csv) for "
                   << recorded << " recorded event(s)" << G4endl;
        }

        // Write the per-reaction firing counts binned by time (merged across
        // worker threads by Run::Merge -> ReactionCounter::Merge).
        ReactionCounter *reactionCounter = masterRun->GetReactionCounter();
        if (reactionCounter != nullptr)
        {
            std::ofstream reactionsOut(OutputDir::Resolve("Reactions.Txt"));
            reactionCounter->WriteAscii(reactionsOut);
            reactionsOut.close();

            G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
            analysisManager->SetDefaultFileType("csv");
            reactionCounter->WriteCsv(analysisManager);

            std::ofstream metadataOut(OutputDir::Resolve("ReactionsMetadata.csv"));
            reactionCounter->WriteMetadata(metadataOut);
            metadataOut.close();

            reactionCounter->Clear();

            DnaLogger::Print(DnaLogger::Level::Info,
                              "[RunAction] reaction counts written (Reactions.Txt / "
                              "Reactions_nt_reactions.csv / ReactionsMetadata.csv)");
        }

        // Write the total energy deposited in the simulation volume (merged
        // across worker threads by Run::Merge; accumulated per-step by
        // ScoreSpecies::ProcessHits into Run::fSumEne).
        std::ofstream energyOut(OutputDir::Resolve("EnergyDeposit.Txt"));
        energyOut << "Total energy deposited in simulation volume: "
                  << G4BestUnit(masterRun->GetSumDose(), "Energy") << "\n";
        energyOut.close();

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] energy deposit written (EnergyDeposit.Txt)");

        // Write the physical-stage interaction firing counts (merged across
        // worker threads by Run::Merge -> PhysicsInteractionCounter::Merge).
        PhysicsInteractionCounter *interactionCounter = masterRun->GetInteractionCounter();
        if (interactionCounter != nullptr)
        {
            std::ofstream interactionsOut(OutputDir::Resolve("PhysicsInteractions.Txt"));
            interactionCounter->WriteAscii(interactionsOut);
            interactionsOut.close();

            std::ofstream interactionsCsv(OutputDir::Resolve("PhysicsInteractions.csv"));
            interactionCounter->WriteCsv(interactionsCsv);
            interactionsCsv.close();

            interactionCounter->Clear();

            DnaLogger::Print(DnaLogger::Level::Info,
                              "[RunAction] physical interaction counts written "
                              "(PhysicsInteractions.Txt / PhysicsInteractions.csv)");
        }
    }
```

new:
```cpp
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
        RunAccumulator::Accumulate(masterRun->GetSumDose(), *masterRun->GetReactionCounter(),
                                    *masterRun->GetInteractionCounter());

        DnaLogger::Print(DnaLogger::Level::Info,
                          "[RunAction] accumulated this run's energy/reaction/interaction data -- "
                          "use /run/dumpDataAndReset to write everything to files");
    }
```

**Step 4 — wire `sim.cc`.** Add the include, right after
`#include "OutputDirMessenger.hh"`:

old:
```cpp
#include "OutputDir.hh"
#include "OutputDirMessenger.hh"
#include "PhysicsList.hh"
```
new:
```cpp
#include "OutputDir.hh"
#include "OutputDirMessenger.hh"
#include "PhysicsList.hh"
#include "RunAccumulatorMessenger.hh"
```

Instantiate the messenger, right after the existing `outputDirMessenger`
line:

old:
```cpp
  // Exposes "/run/outputDir <path>" as a macro-file counterpart to --dir
  OutputDirMessenger *outputDirMessenger = new OutputDirMessenger();
```
new:
```cpp
  // Exposes "/run/outputDir <path>" as a macro-file counterpart to --dir
  OutputDirMessenger *outputDirMessenger = new OutputDirMessenger();
  // Exposes "/run/dumpDataAndReset [prefix]" to flush accumulated
  // species/reaction/interaction/energy data to disk and reset it
  RunAccumulatorMessenger *runAccumulatorMessenger = new RunAccumulatorMessenger();
```

Add the exit-time safety-net call and update cleanup, right before
`delete runManager;`:

old:
```cpp
  // Stop the benchmark here
  theTimer->Stop();

  G4cout << "The simulation took: " << theTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  // Clean up
  delete theTimer;
  delete dnaLoggerMessenger;
  delete outputDirMessenger;
  delete runManager;

  return 0;
}
```
new:
```cpp
  // Safety net: flush any accumulated data that was never explicitly
  // dumped via /run/dumpDataAndReset, so it isn't silently lost.
  runAccumulatorMessenger->FlushIfPending("EndOfRun_");

  // Stop the benchmark here
  theTimer->Stop();

  G4cout << "The simulation took: " << theTimer->GetRealElapsed() << " s to run (real time)"
         << G4endl;

  // Clean up
  delete theTimer;
  delete dnaLoggerMessenger;
  delete outputDirMessenger;
  delete runAccumulatorMessenger;
  delete runManager;

  return 0;
}
```

**Step 5 — build and manually verify** per the Verification Step above.

**Step 6 — update `CLAUDE.md`.** Three edits:

(a) Replace the three paragraphs describing the per-run species/
reactions/energy/physics-interaction output (currently the paragraphs
starting `Each run writes` / `Each run also writes Reactions.Txt` /
`Each run also writes EnergyDeposit.Txt`, i.e. lines 24-39) with:

```markdown
Species/reaction/energy/physical-interaction data accumulates across `/run/beamOn`
calls rather than being written and reset after every run — species yields
accumulate automatically (the `ScoreSpecies` scorer is itself a persistent,
SD-registered object), while energy deposit and the two counters accumulate via
`RunAccumulator` (`src/RunAccumulator.cc`), fed once per run from
`RunAction::EndOfRunAction`. Nothing is written to disk until a macro issues
`/run/dumpDataAndReset [prefix]` (`Idle` state, i.e. between `beamOn` calls),
which writes everything accumulated since the last dump (or program start) and
resets all counters to empty/zero:

- `Species.Txt` (human-readable species yields vs. time) and two CSV ntuples,
  `Species_nt_species.csv` (aggregate sumG/sumG2 per species/time) and
  `Species_nt_species_all.csv` (same, per event); in MT mode the per-event
  pre-chemical dumps are still written continuously as
  `output_event_t<thread>_e<event>.txt` (unaffected by dump/reset).
- `Reactions.Txt`, `Reactions_nt_reactions.csv`, and `ReactionsMetadata.csv`:
  per-time-bin firing counts of each bimolecular reaction (counted live in
  `TimeStepAction::UserReactionAction` via `ReactionCounter`, merged across
  threads in `Run::Merge`, accumulated across runs by `RunAccumulator`).
  `Reactions_nt_reactions.csv` rows are `(reactionId, time, count)`;
  `ReactionsMetadata.csv` maps each `reactionId` to its full `"A + B -> C + D"`
  label (`Reactions.Txt` still prints the full label directly). Acid-base/
  scavenger reactions are not counted (they never reach that hook). The
  default time-bin edges are a built-in 7-entry table; override them with
  `/chem/reaction/timeBinsFixed <width> <unit>` (a fixed step, expanded up to
  the chemistry scheduler's end time) or `/chem/reaction/timeBinsList <e1>
  <e2> ... <eN> <unit>` (explicit edges) — both `PreInit`, mutually exclusive
  (last one issued wins).
- `EnergyDeposit.Txt` (total energy deposited in the simulation volume,
  human-readable).
- `PhysicsInteractions.Txt`/`PhysicsInteractions.csv` (per-process physical-
  interaction firing counts — totals only, no time binning; only discrete
  G4DNA physics processes are counted, e.g. `e-_G4DNAIonisation`,
  `e-_G4DNAExcitation`, `e-_G4DNAElastic`, `e-_G4DNAVibExcitation`,
  `e-_G4DNAAttachment` — `Transportation` and other bookkeeping steps are
  excluded). `PhysicsInteractionCounter` is recorded live per step by
  `SteppingAction`, merged across worker threads in `Run::Merge`, and
  accumulated across runs by `RunAccumulator`, same as the reaction counts.

`prefix` (optional, default none) is prepended literally to every filename
above — no separator is inserted, so pass e.g. `run1_` if you want one.
Reusing a prefix already used earlier in the same `sim.exe` process is a
fatal error (`RunAccumulator::TryReservePrefix`), to catch accidental
overwrites. If a macro never issues `/run/dumpDataAndReset` and there is
still accumulated data pending when the program is about to exit, a
safety-net flush fires automatically with the fixed prefix `EndOfRun_` (see
`RunAccumulatorMessenger::FlushIfPending`, called from `sim.cc` just before
the run manager is destroyed) — so data is never silently lost even if the
operator forgets the manual call. The `[RunAccumulatorMessenger] dumped and
reset...` line is a plain, always-visible `G4cout` line (unlike most of this
project's diagnostics, which go through `DnaLogger` and are silent by
default) — see `RunAccumulatorMessenger.cc`.
```

(b) In the `## Key Files` list, update the `OutputDir.cc` bullet to
mention the prefix (append this sentence to the end of the existing
bullet, before the final period): `" It also holds a separate filename
prefix (SetPrefix()/gPrefix, distinct from the directory) prepended
before the directory join -- set by RunAccumulatorMessenger around each
/run/dumpDataAndReset flush, not exposed as its own macro command."`

Then add two new bullets right after the existing
`PhysicsInteractionCounter.cc` bullet:

```markdown
- `src/RunAccumulator.cc`: process-wide accumulator for energy deposit and the two counters (`ReactionCounter`, `PhysicsInteractionCounter`), persisting across `/run/beamOn` calls — pure logic, no Geant4-kernel dependency, covered by `test/RunAccumulatorTest.cc`. Fed once per run from `RunAction::EndOfRunAction`; species yields don't need this (the `ScoreSpecies` scorer is itself persistent).
- `src/RunAccumulatorMessenger.cc`: exposes `/run/dumpDataAndReset [prefix]` (`Idle` state) — writes everything accumulated since the last dump (species via a direct `ScoreSpecies` lookup, the rest via `RunAccumulator`), then resets it; `prefix` is prepended literally to every output filename via `OutputDir::SetPrefix`. Refuses to reuse a prefix already used earlier in the process (fatal `G4Exception`). Also exposes `FlushIfPending()`, called once from `sim.cc` right before the run manager is destroyed, as a safety net (fixed prefix `EndOfRun_`) for data never manually flushed.
```

(c) In `## Macro and Logging`, insert `` `/run/dumpDataAndReset
[prefix]` (dump-and-reset accumulated output, see above), `` into the
`Common macro controls include` sentence, right after `` `/run/outputDir
<path>` (macro-file counterpart to `--dir`, see above), ``.

**Step 7 — update `.claude/.claude-project.json`.** Three edits inside
the `"run"` object:

Replace:
```json
    "successMarkers": [
      "[RunAction] species yields written",
      "[RunAction] energy deposit written",
      "[RunAction] physical interaction counts written",
      "The simulation took"
    ],
```
with:
```json
    "successMarkers": [
      "[RunAccumulatorMessenger] dumped and reset",
      "The simulation took"
    ],
```

Replace:
```json
    "benignWarnings": [
      "Analysis_W001 (Ntuple filename ... already in use, renamed *_bis.csv): several /run/beamOn in one macro"
    ]
```
with:
```json
    "benignWarnings": []
```

(Output is now only written by an explicit `/run/dumpDataAndReset`, or
once at exit -- the prefix-uniqueness guard refuses a repeated dump
outright instead of silently renaming to `*_bis.csv`, so this warning
can no longer occur under normal use.)

Replace the `"smoke"` object's `"note"`:
```json
    "smoke": {
      "gun": "/gun/particle e-, /gun/energy 25 keV",
      "beamOn": 10,
      "note": "Write a scratch macro into <runBuildDir>/macro (untracked). Do not edit macro/beam.in (100 keV: about 30 min Serial for 6 events)."
    },
```
with:
```json
    "smoke": {
      "gun": "/gun/particle e-, /gun/energy 25 keV",
      "beamOn": 10,
      "note": "Write a scratch macro into <runBuildDir>/macro (untracked). Do not edit macro/beam.in (100 keV: about 30 min Serial for 6 events). End the macro with '/run/dumpDataAndReset' (no prefix) so the files in run.outputs are produced with their bare names -- omitting it falls back to the EndOfRun_-prefixed safety-net dump at process exit instead."
    },
```

**Step 8 — update `.claude/geant4-instructions.md`.** Replace:
```markdown
Several `/run/beamOn` in one macro can overwrite text outputs and rename CSVs
(`*_bis.csv`). For a clean check of one run, use a macro with a single `beamOn`.
```
with:
```markdown
Output files are only written when a macro issues `/run/dumpDataAndReset`
(or, if it never does, once automatically at process exit under the
`EndOfRun_` prefix) -- several `/run/beamOn` calls between dumps just
accumulate into that one write. Reusing a prefix within the same process is
a fatal error rather than a silent `*_bis.csv` rename; give each
`/run/dumpDataAndReset` call in a macro a distinct prefix if you want
separate output per segment.
```

**Step 9 — commit:**

```bash
git add header/RunAccumulatorMessenger.hh src/RunAccumulatorMessenger.cc \
        src/RunAction.cc sim.cc CLAUDE.md \
        .claude/.claude-project.json .claude/geant4-instructions.md
git commit -m "feat: add /run/dumpDataAndReset macro command with exit-time safety net"
```
