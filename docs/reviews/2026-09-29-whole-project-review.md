# dnachem-min review — whole project (branch `O2_Included`)
**Target:** whole project, clean tree (excluding `build*/`, `compile_commands.json`, `.work/`, `.scratch/`, `docs/visual/`) · **Commit:** 0b21210 · **Date:** 2026-09-29
**Environment:** C++20 · Geant4 11.4.1 (`geant4-11-04-patch-01`) · Serial by default, MT with `--threads N` (`G4RunManagerType::MT`) · DNA physics: `G4EmDNAPhysics` (option 0), G4_WATER · Chemistry: `DnaChemistryList` (`G4ChemDissociationChannels_option1` molecules, SBS hard-coded, `PureWater` Chemistry by default, bulk reactions through `G4DNAScavengerProcess`, end time 1 µs)

## Summary
The physics and chemistry wiring matches the Geant4-DNA examples it is modelled on: the rate constants, units and equilibrium types of the bulk reactions are the same as in the UHDR example's `ChemPureWaterBuilder`, and the species G-values are normalised per event as in chem4–chem6. The main defect is in Serial mode, which is the default. The live reaction and physics-interaction counters are never cleared, so every `/run/beamOn` re-adds all earlier runs. This inflates `Reactions.*` and `PhysicsInteractions.*` for the default `beam.in` (two `beamOn`) and for every per-energy dump from `reactions.in`. The existing smoke output fits this model to within 1 %. The per-event `output_event_*.txt` dump is also wrong: it holds the next event's data under the previous event's name, and only one file per thread is ever written. Fix **F-001** first, then **F-002**.

## Architecture tour
- `sim.cc` `main()`: fixed seed 12345, `ArgParser` (`--threads`, `--dir`), `G4RunManagerFactory` (Serial unless `--threads N`), then `DetectorConstruction`, `PhysicsList` and `ActionInitialization`. Batch runs `macro/<arg>`, and `FlushIfPending("EndOfRun_")` runs before the run manager is deleted.
- `DetectorConstruction`: one G4_WATER box. Its size comes from `DnaChemistryWorld` (`/chem/env/halfBox`, default 500 µm), which also provides the diffusion boundary and the bulk composition (pH, scavengers). `ConstructSDandField` registers an `mfDetector` on `World` with `PrimaryKiller` (defaults: kill at 500 keV loss, abort at 1 MeV) and `ScoreSpecies`.
- `PhysicsList`: `G4EmDNAPhysics` plus `DnaChemistryList`, driven directly. `DnaChemistryList::ConstructProcess` extends Sanche to 0.025 eV, adds solvation, Brownian transport for all molecules, H2O dissociation and recombination, and per-molecule `G4DNAScavengerProcess` built from the selected Chemistry's bulk list. It then calls `G4DNAChemistryManager::Initialize()` (the master builds the reaction table and populates the bulk composition) and installs `G4DNAScavengerMaterial` in each thread.
- `ActionInitialization::Build`: primary gun, Run/Event/Stacking/Tracking/Stepping actions, and the `TimeStepAction`. `SetEndTime(1 µs)` and the molecule counters (reset before every event and run) are also set here. `BuildForMaster` creates the `RunAction` and the molecule counters.
- Chemistry is started from `StackingAction::NewStage`. `TimeStepAction::UserReactionAction` counts bimolecular firings into a `ReactionCounter` using time bins. `SteppingAction` counts discrete `G4DNA*` process firings.
- `Run` sums the energy deposit from the `Species` hits map and keeps pointers to the live `TimeStepAction`/`SteppingAction` counters. `Run::Merge` (MT only) absorbs the worker `ScoreSpecies` and counters, then clears them. `RunAction::EndOfRunAction` (master) folds the run into `RunAccumulator` and records one manifest entry.
- `/run/dumpDataAndReset[ToDir]` (`RunAccumulatorMessenger`) writes `Species.*`, `Reactions.*`, `PhysicsInteractions.*` and `Manifest.json`, then resets.

## Findings

### F-001 — Serial mode re-counts earlier runs in Reactions and PhysicsInteractions
- **Severity:** Critical
- **Area:** MT (run merging), affects output in the default Serial mode
- **Location:** `src/actions/Run.cc:29-37` (`Run::Run`), `src/actions/Run.cc:96-103` (`Run::Merge`), `src/actions/RunAction.cc:90-92` (`RunAction::EndOfRunAction`)
- **Confidence:** Confirmed
- **Problem:** In Serial mode, `Run::fReactionCounter` and `Run::fInteractionCounter` point at the *live* counters owned by `TimeStepAction` and `SteppingAction`. These counters persist across `/run/beamOn` and across dumps. They are only cleared in `Run::Merge`, which the Serial run manager never calls. `EndOfRunAction` then merges the cumulative live counters into `RunAccumulator` after every run. Run *k* of *N* is counted (*N−k+1*) times within a dump, and every later dump also contains everything counted since the process started. Species and energy deposit are correct because they use their own reset paths. `macro/beam.in:31,33` (`beamOn 4` then `beamOn 2`, the default macro) and `macro/reactions.in` → `sub_run.mac` (one dump per energy) both give wrong reaction and interaction counts, with no warning. MT is not affected, because Merge clears the worker counters.
- **Evidence:**
  ```cpp
  // Run.cc:31-37 — Serial: points at the live, never-cleared counters
  fReactionCounter = (timeStepAction != nullptr) ? &timeStepAction->GetReactionCounter() : &fOwnedReactionCounter;
  fInteractionCounter = (steppingAction != nullptr) ? &steppingAction->GetInteractionCounter() : &fOwnedInteractionCounter;
  // Run.cc:99-103 — the only Clear(), MT Merge only
  fReactionCounter->Merge(*localRun->fReactionCounter);
  localRun->fReactionCounter->Clear();
  ```
  Existing smoke output `.scratch/tests/2026-09-29__run-manifest-file/smoke/m1` (Serial) has these dumps: dump 1 = run 0 (2×10 keV) + run 1 (25 keV), dump 2 = run 2 (10 keV), dump 3 = run 3 (10 keV). The `e-_G4DNAIonisation` counts are 3328, 2804 and 3301. If ionisations scale with energy (*x* per keV), the cumulative model predicts 65x, 55x and 65x. With x = 51.2 that gives 3328, 2816 and 3328, within 1 % of the observed values. Correct counting would give 45x, 10x and 10x, so dumps 2 and 3 would be about 4.5× smaller than dump 1 instead of the same size.
- **Fix direction:** Give each thread's counters the lifetime of a run. For example, in `RunAction::BeginOfRunAction`, on every thread that owns live counters (`!IsMaster()` in MT, or any thread in Serial), clear the `TimeStepAction` and `SteppingAction` counters. Keep the MT clear-after-merge or remove it, but make sure there is exactly one reset per run. Alternatively, `Run` could own the counters by value and the actions could write into the current `Run`'s counters, which removes the shared lifetime entirely.
- **Verify with:** a Serial scratch macro at 10 keV: `beamOn 2`, `/run/dumpDataAndReset A_`, `beamOn 2`, `/run/dumpDataAndReset B_`. `B_PhysicsInteractions.Txt` should match `A_` within statistical tolerance (see Testing gaps), not be about 2× larger. Repeat with `--threads 2` to confirm MT is unchanged.
- **Related:** —

### F-002 — `output_event_*.txt` holds the next event's data, and only one file per thread is ever written
- **Severity:** High
- **Area:** API
- **Location:** `src/chemistry/TimeStepAction.cc:171-180` (`TimeStepAction::EndProcessing`), `src/chemistry/TimeStepAction.cc:198-207` (`TimeStepAction::DumpPreChemical`)
- **Confidence:** Confirmed
- **Problem:** `EndProcessing` for event *N* calls `G4DNAChemistryManager::WriteInto("output_event_N.txt")`. In Geant4 11.4.1, `FormattedText::WriteInto` opens the stream and sets `fFileInitialized = false`. `G4DNAChemistryManager::Run()` then calls `CloseFile()`, which returns early because `!fFileInitialized`, so the stream stays open. The physico-chemical records of event *N+1* (`CreateWaterMolecule`, which calls `InitializeFile()` lazily) are therefore written into `output_event_N.txt`. At the end of event *N+1*, `WriteInto` calls `open()` on an already-open `ofstream`, which fails and sets failbit. Every later write is lost, and no second file is created. The code comment says the file holds "the state at the beginning of the simulation (eventID=0)", which is also wrong. The file is listed as a run output in `.claude/.claude-project.json` (`run.outputs`), but it is not in the manifest's `files`. It also ignores the dump prefix/subdir and is overwritten across runs in Serial, because event IDs restart at 0.
- **Evidence:** Geant4 `G4PhysChemIO.cc:78-83` (`WriteInto`: `fOfstream.open(...); fFileInitialized = false;`), `:94-102` (`CloseFile`: `if (!fFileInitialized) return;`), `:111` (lazy `InitializeFile`), and `G4DNAChemistryManager.cc:331-333` (`Process(); CloseFile();`). The outputs match: every Serial smoke directory (`.scratch/gtest/e10`, `.../smoke/m1`, 2–6 events) has only `output_event_0.txt` (~150 kB). MT `.scratch/gtest/fix/mt8` has one file per thread that ran events (`output_event_t0_e2.txt`, `output_event_t1_e0.txt`).
- **Fix direction:** Decide whether the file is wanted (Q1). If it is, call `WriteInto(file_N)` at *begin* of event *N* (e.g. `EventAction::BeginOfEventAction`, before the physics stage), and close it explicitly at end of event (`G4DNAChemistryManager::CloseFile()` after `Run()`, or open with a fresh `FormattedText` per event). Include the run ID in the name and route it through the dump subdir if it belongs to a dump. If it is not wanted, remove the `WriteInto` call and drop it from `run.outputs`.
- **Verify with:** 3 events in Serial should produce 3 files. For each file, the first `#Parent ID` block's energy sum should be of order the beam energy, and the H2O record count should match that event's "Size of pre chemical main list" log line.
- **Related:** F-003

### F-003 — `/chem/reaction/dump` file is rewritten concurrently by the master and every worker in MT
- **Severity:** Medium
- **Area:** MT
- **Location:** `src/chemistry/DnaChemistryList.cc:333-337` (`DnaChemistryList::ConstructProcess`)
- **Confidence:** Likely
- **Problem:** `ConstructProcess` runs on the master and again on each worker thread. The code's own comment at `:321-322` says "Runs once (serial) or per worker thread (MT)". The `DnaChemistryList` instance, and so `fReactionDumpFile`, is shared, so every thread opens and writes the same path at nearly the same time during worker initialisation. The resulting file can be interleaved or truncated in MT, and the master copy is overwritten in any case.
- **Evidence:**
  ```cpp
  // DnaChemistryList.cc:335-337 — no thread guard
  if (!fReactionDumpFile.empty()) {
    ReactionTableDump::DumpReactionTable(OutputDir::Resolve(fReactionDumpFile));
  }
  ```
- **Fix direction:** Dump only from the master or in Serial: `if (!fReactionDumpFile.empty() && !G4Threading::IsWorkerThread())`. The master's process table already has the bulk processes registered, so the content is complete there.
- **Verify with:** `sim <macro with /chem/reaction/dump rt.txt> --threads 4` versus the same in Serial. The files should be byte-identical, and the line count should equal the number of bimolecular plus bulk reactions.
- **Related:** F-002

### F-004 — `/primaryKiller/minKineticE` is accepted but has no effect
- **Severity:** Medium
- **Area:** API
- **Location:** `src/scoring/PrimaryKiller.cc:25` (constructor), `:43-61` (`PrimaryKiller::SetNewValue`), `:33-39` (destructor)
- **Confidence:** Confirmed
- **Problem:** The command is created, but `SetNewValue` has no branch for it, so `fKineticE_Min` stays at 0 and the "kill primary below E" feature cannot be turned on from a macro. There is no error or warning. It is also not deleted in the destructor (small leak). The macros advertise it (`macro/beam.in:28`, `macro/beam_o2.in:23`, commented out). The same bug exists in the upstream chem4 example this class came from.
- **Evidence:**
  ```cpp
  fpMinKineticE = new G4UIcmdWithADoubleAndUnit("/primaryKiller/minKineticE", this); // :25
  // SetNewValue handles fpELossUI, fpAbortEventIfELossUpperThan, fpSizeUI, fpVerboseUI only
  ```
- **Fix direction:** Add `else if (command == fpMinKineticE) fKineticE_Min = fpMinKineticE->GetNewDoubleValue(newValue);` and delete the command in the destructor. Also give the energy commands a unit category (`SetUnitCategory("Energy")`).
- **Verify with:** 100 keV, `/primaryKiller/minKineticE 90 keV`, `beamOn 5`. `energyDeposit_eV` per event in `Manifest.json` should drop well below 100 keV.
- **Related:** F-006

### F-005 — Stale time-step-model and end-time comments, and the debug log reports the wrong model
- **Severity:** Low
- **Area:** maintainability
- **Location:** `macro/beam.in:7-8`, `macro/beam_02.in:8-9`, `src/physics/PhysicsList.cc:24-26`, `src/actions/StackingAction.cc:44-46`, `CLAUDE.md` (Testing section)
- **Confidence:** Confirmed
- **Problem:** Several comments say "DnaChemistryList rejects IRT with a fatal exception; IRT_syn is allowed". `DnaChemistryList::ConstructTimeStepModel` (`DnaChemistryList.cc:258-264`) always registers SBS and never reads `G4EmParameters`, so IRT and IRT_syn are both silently ignored, as CLAUDE.md states. The Debug log in `StackingAction` prints `G4EmParameters::GetTimeStepModel()`, which reports "IRT" if a macro asked for it, while SBS actually runs. The time-step model is not recorded in `Manifest.json` either. Separately, CLAUDE.md says `ActionInitialization::Build()` "runs on `/run/initialize`". In Serial it actually runs inside `SetUserInitialization` (`sim.cc:113`; Geant4 `G4RunManager.cc:974-977`), before the macro starts. The documented override order still works, but the stated reason is wrong for the default mode.
- **Evidence:** `PhysicsList.cc:24-26`: "DnaChemistryList rejects IRT with a fatal exception; a macro may still select IRT_syn".
- **Fix direction:** Update these comments to "SBS only; `/process/chem/TimeStepModel` is ignored". Log the actual model (always "SBS") or add `"timeStepModel": "SBS"` to the manifest. Correct the CLAUDE.md sentence.
- **Verify with:** reading, plus `grep -rn "IRT_syn" macro src`.
- **Related:** —

### F-006 — Aborted events still count toward PhysicsInteractions
- **Severity:** Low
- **Area:** physics (scoring consistency)
- **Location:** `src/actions/SteppingAction.cc:10-21`, `src/scoring/PrimaryKiller.cc:93-112`, `src/actions/Run.cc:50-51`
- **Confidence:** Likely
- **Problem:** When `PrimaryKiller` aborts an event (primary energy loss > `eLossMax`, default 1 MeV), `Run::RecordEvent` and `ScoreSpecies::EndOfEvent` skip it, so it is excluded from events, energy deposit and species. `SteppingAction` has already recorded that event's steps, though, so interaction counts per event and per eV are inflated. This only matters for beams whose primary loses more than 1 MeV, or when `/primaryKiller/eLossMax` is lowered.
- **Evidence:** `Run.cc:50-51` `if (event->IsAborted()) return;`. There is no equivalent check in the `SteppingAction` path.
- **Fix direction:** Record into a per-event scratch counter and merge it into the live counter from `EventAction::EndOfEventAction` only if `!event->IsAborted()`.
- **Verify with:** `/primaryKiller/eLossMax 5 keV` at 100 keV. `events` in the manifest drops, and interaction counts should drop in proportion.
- **Related:** F-004

### F-007 — Global mutex taken on every chemical reaction in MT
- **Severity:** Low
- **Area:** performance
- **Location:** `src/scoring/ReactionCounter.cc:115-124` (`ReactionCounter::BinFor`)
- **Confidence:** Likely
- **Problem:** `BinFor` locks the process-wide `gTimeBinsMutex` for every recorded reaction on every worker. The only writer is `ConfigureBinEdges`, called from the master's `BeginOfRunAction` while workers are idle. With many threads and high reaction counts (UHDR-like tracks), this serialises the hot path for no benefit.
- **Evidence:** `std::lock_guard<std::mutex> lock(gTimeBinsMutex); for (G4double edge : gTimeBins) ...`
- **Fix direction:** Copy the edges into each `ReactionCounter` (or a `G4ThreadLocal` snapshot) at the start of the run, and read them without a lock during the run.
- **Verify with:** `--threads 8`, the same macro before and after. Compare wall time; outputs should match statistically.
- **Related:** —

### F-008 — Scorer lookup relies on the hits-collection ID being equal to the primitive index
- **Severity:** Low
- **Area:** API
- **Location:** `src/actions/Run.cc:23-27` (`Run::Run`), `src/actions/Run.cc:90-94` (`Run::Merge`)
- **Confidence:** Confirmed
- **Problem:** `mfdet->GetPrimitive(GetCollectionID("mfDetector/Species"))` passes a global hits-collection ID, which Geant4 returns 0-based (`G4HCtable.cc:43-66`), as an index into the detector's primitive vector. This only works because `mfDetector` is the only SD and `Species` is its second primitive. Adding any SD or primitive before it would make the index wrong. `Run::Merge` then dereferences the unchecked `dynamic_cast` result. The pattern is inherited from chem4 and chem6.
- **Evidence:** `fScorerRun = mfdet->GetPrimitive(CollectionIDspecies);` … `masterScorer->AbsorbResultsFromWorkerScorer(localScorer);` (no null check).
- **Fix direction:** Look the primitive up by name by iterating `GetNumberOfPrimitives()`/`GetPrimitive(i)->GetName() == "Species"`, and raise a `G4Exception` if it is not found.
- **Verify with:** existing smoke run; register a dummy primitive before `Species` and check that the run still merges.
- **Related:** —

### F-009 — Ad hoc console output and dead code
- **Severity:** Low
- **Area:** maintainability
- **Location:** `src/geometry/DetectorConstruction.cc:33-37,154-183`, `src/chemistry/TimeStepAction.cc:72,173-179,214`, `src/actions/EventAction.cc:38-84`, `header/actions/StackingAction.hh:20`, `header/actions/RunAction.hh:29`, `sim.cc:30`
- **Confidence:** Confirmed
- **Problem:** CLAUDE.md requires `DnaLogger` for application logging. `DetectorConstruction` writes 5–7 `std::cerr` lines at start-up. `TimeStepAction` prints 5 unconditional `G4cout` lines per event, including a raw `G4Event*` pointer, and these appear even at `/dnaLogger/verbose Quiet`. Some code is unused: `EventAction::WriteChemistryOutput`/`DumpPreChemical` (never called), `StackingAction::DumpPreChemical` (declared, not defined), `RunAction::fDoseAccumulable`, and the global `std::ofstream out`.
- **Evidence:** `TimeStepAction.cc:178` `G4cout << "[TimeStepAction End] G4Event pointer: " << currentEvent << G4endl;`
- **Fix direction:** Route these through `DnaLogger::Print(Debug|Trace, ...)` and delete the dead members and functions.
- **Verify with:** smoke run at `/dnaLogger/verbose Error` shows no per-event lines; build is still clean.
- **Related:** F-002 (same functions)

## Testing gaps
The project records that fixed-seed output is **not** bit-reproducible (`sim.cc:141-148`, ADR 0005, `.scratch/variability/report.md`). Compare runs statistically: use the mean of 3 baseline runs, a 15 % tolerance, and only quantities with count ≥ 400 (ionisation, elastic and solvation counts, and key reactions by label). Run `sim` from `build/` (RelWithDebInfo) with a scratch macro in `build/macro/`, 10 keV e-, as `.claude/geant4-instructions.md` §3–4 describes. Unit tests go in `build-ninja/` via `ctest` (§5).

1. **Two dumps, same beam** (catches F-001). Serial: `beamOn 2` → `/run/dumpDataAndReset A_` → `beamOn 2` → `/run/dumpDataAndReset B_`. Check that `B_` ≈ `A_` for `PhysicsInteractions` and the top reactions (ratio within 15 %), not about 2×. Repeat with `--threads 2`. Also useful as a regression check for `beam.in`'s two `beamOn`.
2. **Counter lifetime unit test** (catches F-001 at logic level). If the fix moves the per-run reset into a small pure helper (e.g. "take this run's delta and clear the live counter"), add a `RunAccumulatorTest` case: record, fold, record again, fold. The accumulated total must equal the sum of the two recordings.
3. **Per-event pre-chemical file** (catches F-002). 3 events should give 3 files, and each file's H2O record count should match that event's "Size of pre chemical main list" line.
4. **MT reaction-table dump** (catches F-003). `/chem/reaction/dump` in Serial and with `--threads 4`; `diff` must be empty.
5. **PrimaryKiller controls** (catches F-004, F-006). `minKineticE` lowers `energyDeposit_eV`/event, and `eLossMax` aborts reduce `events` and interaction counts in proportion.
6. **Chemistry off / other DNA options.** Not currently switchable (SBS and `G4EmDNAPhysics` are hard-coded by design), so there is no gap to fill beyond the above.

## Suggested sessions

### S1 — serial-counter-reset
- **Findings:** F-001
- **Goal:** Reactions and PhysicsInteractions count each run exactly once in both Serial and MT.
- **Why together:** single root cause (counter lifetime across runs).
- **Depends on:** none

### S2 — diagnostic-output-files
- **Findings:** F-002, F-003
- **Goal:** Per-event pre-chemical files and the reaction-table dump are correctly named, complete, and written once per file.
- **Why together:** both are auxiliary output files written from per-thread chemistry code paths.
- **Depends on:** Q1 answered

### S3 — primary-killer-controls
- **Findings:** F-004, F-006
- **Goal:** Every `/primaryKiller/*` command works, and aborted events are excluded consistently from all outputs.
- **Why together:** same scorer and event-abort path.
- **Depends on:** S1 (both touch how the live interaction counter is fed)

### S4 — cleanup-and-docs
- **Findings:** F-005, F-007, F-008, F-009
- **Goal:** Comments and docs match behaviour, logging goes through DnaLogger, the scorer lookup is robust, and there is no lock on the reaction hot path.
- **Why together:** low-risk local changes with no effect on physics results.
- **Depends on:** S2 (F-009 touches the same `TimeStepAction` functions)

## Open questions
- **Q1 — Is `output_event_*.txt` used by any analysis? (F-002)** If yes, it needs the per-event open/close fix and should probably live inside the dump's folder. If no, removing it is simpler and also removes per-event file I/O.
- **Q2 — Should Reactions.Txt include bulk reactions? (F-001 context)** Bulk (`G4DNAScavengerProcess`) reactions are not seen by `UserReactionAction`, so the counts are bimolecular only, as CLAUDE.md states. With `/chem/env/scavenger O2 21 %`, e_aq/H/O⁻ + O2(B) are major channels and the acid-base buffer fires constantly, so the reaction output hides them. Confirm this is intended, or decide whether a bulk-reaction counter is in scope.
- **Q3 — Should `BoscoloChem` be selectable while it is an exact copy of `PureWater`?** The file header marks it as work in progress. A run with `/chem/select BoscoloChem` produces PureWater results under `"chemistry": "BoscoloChem"` in the manifest (ADR 0002).
- **Q4 — Per-run `seed` in the manifest (ADR 0005).** `Run::fSeed` is `engine->getSeed()`, the last *configured* seed. Consecutive runs without `/random/setSeeds` report the same seed even though they use different random streams, which looks like a replay key but is not one. ADR 0005 accepts "seed as configured". Consider renaming the key (e.g. `configuredSeed`) or also storing the engine status.

## Not reviewed
- Geant4 internals beyond the call sites checked (`G4DNAChemistryManager`, `G4PhysChemIO`, `G4HCtable`, `G4RunManager::SetUserInitialization`, `G4Scheduler` defaults, UHDR `ChemPureWaterBuilder`).
- Nothing was built or run for this review. The evidence comes from reading the code and from existing smoke logs and outputs in `.scratch/`. MT was not exercised beyond those logs.
- Unit-test sources under `test/` (only their CMake wiring), `src/scoring/DataNode.cc` beyond its use by `JsonWriter`, and `BoscoloChemReactions.cc` beyond a diff (identical to `PureWater` apart from names).
- Physical correctness of individual rate constants: they were checked for consistency with the UHDR example, not against the primary literature.
- `useGUI` path (`sim.cc:119-140`), which is disabled by a compile-time constant and references a `macro/vis.mac` that does not exist.
- `.work/`, `.kb-notes/`, `docs/visual/`, `.claude/` tooling, `compile_commands.json`.
