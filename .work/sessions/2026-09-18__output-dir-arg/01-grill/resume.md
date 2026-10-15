# Session: output-dir-arg

**Date:** 2026-09-18T12:44:37.543Z
**Status:** Grill phase complete

## Problem Statement

`sim.exe` always writes its output (`Species.Txt`, the `Species_nt_species*.csv` ntuples,
per-thread/per-event `output_event_t<thread>_e<event>.txt` pre-chemical dumps, and — when a
macro issues `/chem/reaction/dump <filename>` — the reaction-table dump) into whatever the
process's current working directory happens to be, with no way to redirect it. This makes
automated testing (an AI agent driving many `sim.exe` runs) awkward: every run's output has to
be isolated by `cd`-ing into a fresh working directory rather than by pointing the run at a
directory directly, and parallel/sequential runs risk colliding on the same filenames.

## Context & Constraints

- **Current behavior:** Output filenames are hardcoded, cwd-relative literals scattered across
  four sites: `ScoreSpecies::ASCII()` (`Species.Txt`, `src/ScoreSpecies.cc:276`),
  `ScoreSpecies::WriteWithAnalysisManager()` (`analysisManager->OpenFile("Species")`,
  `src/ScoreSpecies.cc:324`, producing the `_nt_species(_all).csv` files),
  `TimeStepAction::DumpPreChemical()` (`output_event_[t<n>_e]<id>.txt`,
  `src/TimeStepAction.cc:176/179`), and `DnaChemistryList::ConstructProcess()`'s reaction-table
  dump (`src/DnaChemistryList.cc:239`, filename comes from the macro-set `fReactionDumpFile`
  via `/chem/reaction/dump`).
- **Pain point:** No CLI-level control over where any of this lands; every output site assumes
  cwd.
- **Dependencies:** `sim.cc` already has an `ArgParser` (`header/ArgParser.hh`,
  `src/ArgParser.cc`) registering `--threads`; adding `--dir` is a second flag on the same
  parser. `DnaLogger` (`header/DnaLogger.hh`) is the existing precedent for a process-wide,
  set-once-in-`main()`-before-thread-creation global read from any MT worker thread without
  extra synchronization.
- **Tech stack:** Geant4 (`G4String`, `G4AnalysisManager`, `G4DNAChemistryManager`), C++20
  (`<filesystem>` available per project build requirements).

## Success Metrics

- `./sim beam.in --dir <path>` writes `Species.Txt`, the CSV ntuples, the per-event/per-thread
  pre-chemical dumps, and (if `/chem/reaction/dump` is used in the macro) the reaction-table
  dump all under `<path>` instead of cwd.
- Omitting `--dir` preserves today's behavior exactly (all outputs cwd-relative, unchanged
  filenames).
- `<path>`'s immediate parent must already exist; `<path>` itself is created if missing.
  Missing parent, or `<path>` existing as a non-directory, is a startup error (matches the
  existing `--threads` validation-then-`exit(1)` pattern in `main()`).
- `test/OutputDirTest.cc` (plain-assert, no-Geant4-runtime style, matching `ArgParserTest.cc`)
  covers: empty-dir passthrough, successful creation, already-exists-as-directory (ok),
  already-exists-as-file (error), missing-parent (error), and `Resolve()` path-joining.

## Architecture & Approach

Add a process-wide `OutputDir` namespace (`header/OutputDir.hh` / `src/OutputDir.cc`), mirroring
`DnaLogger`'s global-state shape but simpler — no atomics needed, since the value is set exactly
once in `main()` before `G4RunManagerFactory::CreateRunManager()` spawns any worker threads
(happens-before via thread creation covers visibility to every MT worker thereafter):

- `G4bool Configure(const G4String &dir, G4String &err)` — no-op success if `dir` is empty.
  Otherwise creates only `dir` itself via non-recursive `std::filesystem::create_directory`
  (its parent must already exist). Returns `false` and fills `err` if the parent is missing or
  `dir` exists as a non-directory.
- `G4String Resolve(const G4String &filename)` — joins the configured directory with
  `filename`, or returns `filename` unchanged if `Configure` was never called with a non-empty
  directory (default cwd-relative behavior).

**`sim.cc` wiring:** register `argParser.AddStringFlag("--dir")`; immediately after
`argParser.Parse(...)` succeeds, call `OutputDir::Configure(argParser.GetString("--dir"), err)`
and `exit(1)` via the same `DnaLogger::Print(Level::Error, ...)` path used for `--threads`
failures, before `CreateRunManager`.

**Redirected call sites** — each wraps its filename in `OutputDir::Resolve(...)`:
- `ScoreSpecies.cc:276` (`Species.Txt`)
- `ScoreSpecies.cc:324` (`analysisManager->OpenFile(...)`, covers both CSV ntuples)
- `TimeStepAction.cc:176` and `:179` (per-event/per-thread pre-chemical dumps)
- `DnaChemistryList.cc:239` (`ReactionTableDump::DumpReactionTable(fReactionDumpFile)`)

## Assumptions & Trade-offs

- Directory creation is non-recursive by explicit choice (confirmed with user): `--dir` creates
  only the leaf directory, not missing parents — closer to `mkdir` than `mkdir -p`, catching
  typo'd paths at startup rather than silently building a deep tree.
- `--dir` also relocates the `/chem/reaction/dump` target (confirmed with user), even though
  that filename is already explicitly chosen in the macro — every output from a run should be
  reachable under one directory when `--dir` is passed, rather than leaving one file exempt.
- Not doing: no per-output override (e.g. no way to send `Species.Txt` to one directory and the
  CSVs to another) — one `--dir` covers every output uniformly, matching the "isolate one run's
  files in one directory" motivation. No change to GUI-mode (`useGUI = true`) output handling —
  out of scope, GUI runs aren't the automated-testing use case this targets.
- This is bounded work: existing flow (`sim.cc`'s `ArgParser` usage, the four output call
  sites) is being extended, not restructured; no new subsystem.

## Open Questions

None outstanding — both open design questions (auto-create semantics; whether `--dir` covers
the reaction-table dump) were resolved with the user during grilling.

## Notes

- Precedent for a pure, no-Geant4-runtime-dependent utility with its own plain-assert test file:
  `ArgParser.hh`/`.cc` + `test/ArgParserTest.cc`. `OutputDir` follows the same shape.
- Precedent for process-wide global state set once in `main()` and read from any MT worker
  thread: `DnaLogger` (`header/DnaLogger.hh`), though `OutputDir` doesn't need `DnaLogger`'s
  `std::atomic` since it's never mutated after the pre-thread-creation `Configure()` call.

## Token Usage

- **Input:** 74
- **Output:** 24641
- **Cache read:** 3168322
- **Cache creation:** 69064
- **Total:** 3262101
