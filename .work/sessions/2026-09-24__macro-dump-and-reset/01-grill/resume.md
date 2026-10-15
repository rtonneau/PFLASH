# Session: macro-dump-and-reset

**Date:** 2026-09-24T06:32:18.337Z
**Status:** Grill phase complete

## Problem Statement

`dnachem-min` currently writes its per-run output files (`Species.Txt`/CSVs,
`Reactions.Txt`/CSV/metadata, `EnergyDeposit.Txt`, `PhysicsInteractions.Txt`/CSV)
and clears their underlying counters automatically at the end of every
`/run/beamOn`, with multiple `beamOn`s in one macro producing overwritten/`_bis`
file variants. There is no way for a macro to control *when* data gets flushed
to disk or to label a flush with a custom filename prefix. We want a macro-level
command that dumps all current data to files and resets the counters on demand,
optionally prefixing every output filename, so a macro can freely batch several
`beamOn`s into one accumulated statistics window and flush it under a name of
its choosing.

## Context & Constraints

- Only `ScoreSpecies` (species yields) is currently a persistent, SD-registered
  object that survives across `/run/beamOn` calls. Energy deposit
  (`Run::fSumEne`) and the two counters (`ReactionCounter`,
  `PhysicsInteractionCounter`) as exposed via `Run::GetSumDose()` /
  `GetReactionCounter()` / `GetInteractionCounter()` live on the per-run `Run`
  object, which `RunAction::GenerateRun()` recreates fresh every `beamOn` — so
  today they are implicitly reset every run regardless of writing.
- `RunAction` itself (both master and per-worker instances) is created once per
  thread in `ActionInitialization::BuildForMaster()`/`Build()` and persists for
  the whole session — the natural home for persistent, cross-run accumulators.
- Every existing output file site already funnels through
  `OutputDir::Resolve(filename)` (`ScoreSpecies.cc`, `ReactionCounter.cc`,
  `RunAction.cc`), which is the existing, documented extension point for
  filename manipulation (see project `CLAUDE.md`'s description of
  `OutputDir::Resolve`).
- Messengers in this project (`DnaLoggerMessenger`, `OutputDirMessenger`) are
  instantiated exactly once in `sim.cc::main()`, before any worker thread
  starts, and operate on file-scope static state (e.g. `OutputDir`'s
  `gConfiguredDir`) rather than per-thread objects — this avoids duplicate
  command-tree registration across worker `RunAction` instances.
- Per-project convention (`CLAUDE.md`): unit tests (`test/*Test.cc`, plain
  `assert` + CTest) are built/run from `build-ninja/` (Debug), and must not
  call `G4UnitDefinition::GetValueOf()` (observed to corrupt memory from that
  Debug test binary). Logic that doesn't need a live G4 kernel should stay
  independently testable, following `ReactionCounterTest.cc`'s pattern.

## Success Metrics

- A macro can issue `/run/dumpDataAndReset [prefix]` between `beamOn` calls
  (or at the end of a macro) and get `Species.Txt`/CSVs, `Reactions.Txt`/CSV/
  metadata, `EnergyDeposit.Txt`, and `PhysicsInteractions.Txt`/CSV written with
  every filename prefixed by `prefix` (prepended literally, no inserted
  separator), reflecting everything accumulated since the last dump (or
  program start), then all counters are reset to empty/zero.
- Data accumulated across multiple `beamOn`s without an intervening
  `/run/dumpDataAndReset` is no longer written/cleared automatically at each
  run's end — it keeps accumulating until the command fires.
- Reusing a prefix that was already used earlier in the same process is
  refused (fatal `G4Exception`, same style as the existing `--dir` /
  `/run/outputDir` conflict) rather than silently overwriting prior output.
- If the macro never calls `/run/dumpDataAndReset` and pending accumulated
  data exists when the program is about to exit, a safety-net flush fires
  automatically using a fixed prefix (`EndOfRun_`), so data is never silently
  lost even if the operator forgets the manual call.

## Architecture & Approach

1. **`OutputDir` gains a prefix.** Add a file-scope `gPrefix` (mirroring the
   existing `gConfiguredDir`) and `OutputDir::SetPrefix(const G4String&)`.
   `Resolve(filename)` prepends `gPrefix` to `filename` (literal
   concatenation, no auto-inserted separator) before joining it to the
   configured directory. No call sites need to change — every output site
   already goes through `Resolve()`.

2. **New `RunAccumulator` class** (`src/RunAccumulator.cc` /
   `header/RunAccumulator.hh`), static/free-function style like `OutputDir`,
   holding process-wide persistent state:
   - `G4double` accumulated energy
   - one persistent `ReactionCounter`
   - one persistent `PhysicsInteractionCounter`
   - an in-memory `std::set<G4String>` of prefixes already used this process
     (uniqueness guard; not checked against the filesystem, only against
     prefixes seen via this command during the current run of the program)
   - a pending-data flag

   API:
   - `Accumulate(G4double energy, const ReactionCounter&, const
     PhysicsInteractionCounter&)` — merges into the persistent totals and
     marks pending data. Called from `RunAction::EndOfRunAction` (master
     thread only) once per run, replacing the current write-and-clear logic
     for Reactions/Energy/PhysicsInteractions. Species is untouched: it
     already accumulates automatically because the SD-registered
     `ScoreSpecies` scorer is itself persistent and `Run::Merge()` already
     feeds it via `AbsorbResultsFromWorkerScorer`.
   - `DumpAndReset(const G4String& prefix, G4bool enforceUniqueness,
     G4String& err)` — if `enforceUniqueness` and `prefix` was already used,
     sets `err` and returns `false` without writing or clearing anything.
     Otherwise: records `prefix` as used, sets `OutputDir::SetPrefix(prefix)`,
     looks up the persistent `ScoreSpecies` scorer via `G4SDManager` (same
     lookup `Run::Run()` already does) and calls its `ASCII()` +
     `OutputAndClear()`, writes `Reactions.Txt`/CSV/metadata from the
     accumulated `ReactionCounter` then clears it, writes
     `EnergyDeposit.Txt` from the accumulated energy then resets it to 0,
     writes `PhysicsInteractions.Txt`/CSV from the accumulated
     `PhysicsInteractionCounter` then clears it, resets
     `OutputDir::SetPrefix("")`, clears the pending-data flag, and returns
     `true`.
   - `FlushIfPending(const G4String& autoPrefix)` — if pending data exists,
     calls `DumpAndReset(autoPrefix, /*enforceUniqueness=*/false, err)`
     (ignoring `err`, since this path must never fail); no-op otherwise.

3. **New `RunAccumulatorMessenger`** (`src/RunAccumulatorMessenger.cc` /
   header), a `G4UImessenger` instantiated once in `sim.cc::main()` alongside
   `DnaLoggerMessenger`/`OutputDirMessenger`. Exposes
   `/run/dumpDataAndReset [prefix]` via `G4UIcmdWithAString`, parameter name
   `prefix`, omittable with default `""`, `AvailableForStates(G4State_Idle)`.
   `SetNewValue` calls `RunAccumulator::DumpAndReset(value,
   /*enforceUniqueness=*/true, err)`; on failure raises a fatal
   `G4Exception` with `err` as the message (same pattern as
   `OutputDirMessenger`'s conflicting-directory case).

4. **`RunAction::EndOfRunAction`** (`src/RunAction.cc`): remove the direct
   file-writing/clearing blocks for `Reactions.Txt`/CSV/metadata,
   `EnergyDeposit.Txt`, and `PhysicsInteractions.Txt`/CSV; replace with one
   call to `RunAccumulator::Accumulate(masterRun->GetSumDose(),
   *masterRun->GetReactionCounter(), *masterRun->GetInteractionCounter())`.
   Species handling (the `ScoreSpecies` block) is left exactly as-is except
   it must no longer call `scorer->OutputAndClear()` — only accumulation via
   `Run::Merge()` continues to happen automatically; the write+clear now
   happens exclusively inside `RunAccumulator::DumpAndReset`.

5. **`sim.cc`**: include the new headers, instantiate
   `RunAccumulatorMessenger` next to the existing messengers, and — right
   before `delete runManager;`, after both the GUI and batch execution
   branches — call `RunAccumulator::FlushIfPending("EndOfRun_")` as the
   exit-time safety net. Delete the new messenger alongside the others in the
   cleanup block.

6. **Docs**: update the project `CLAUDE.md` "Build and Run" section to
   document `/run/dumpDataAndReset [prefix]`, and correct the existing line
   stating that "Multiple `/run/beamOn` in one macro overwrite `Reactions.Txt`
   and produce `_bis` CSV names" — that automatic per-run write/clear
   behavior goes away with this change.

7. **Tests**: add `test/RunAccumulatorTest.cc` covering the parts that don't
   need a live G4 kernel — prefix-uniqueness refusal (same prefix twice with
   `enforceUniqueness=true` is refused; `enforceUniqueness=false` always
   succeeds), and the pending-data flag transitions (`Accumulate` sets it,
   `DumpAndReset` clears it, `FlushIfPending` is a no-op when nothing is
   pending). The energy/counter arithmetic and the species/file-writing path
   stay integration-level, verified by running `sim.exe`, matching how
   `ReactionCounter`'s bin-edge logic vs. its live-counting behavior are
   already split today.

## Assumptions & Trade-offs

- The prefix uniqueness guard is in-memory only, scoped to the current
  process — it does not check for pre-existing files left on disk from a
  previous invocation of `sim.exe`, and resets every time the program
  restarts. This matches the stated intent (catch accidental prefix reuse
  within one macro run) without adding filesystem-existence-check
  complexity.
- No separator is auto-inserted between the prefix and the base filename;
  the operator is responsible for including e.g. a trailing `_` in the
  prefix they pass if they want one.
- Removing the automatic per-`beamOn` write/clear is a behavior change for
  any existing macro that relied on it (e.g. `beam.in`, `beam_o2.in`, any
  multi-`beamOn` macro) — those macros will need `/run/dumpDataAndReset`
  added, or they'll rely solely on the `EndOfRun_` exit-time safety net
  (which still produces correct, if unprefixed-per-segment, output).
- `RunAccumulator` is project-specific (references `ReactionCounter`,
  `PhysicsInteractionCounter`, `OutputDir`, `ScoreSpecies`/`G4SDManager`)
  and is not designed to be copy-paste portable to another project, unlike
  `PureWaterReactions.cc` / `PhysicsInteractionCounter.cc`.

## Open Questions

None outstanding — design approved by the user, including the command name
`/run/dumpDataAndReset` (in place of the originally proposed
`/run/dumpAndReset`) and the `EndOfRun_` exit-flush sentinel prefix.

## Notes

Classified as a bounded task (extends an existing, already-documented output
pipeline; no new subsystem). Key files to touch: `src/OutputDir.cc`,
`header/OutputDir.hh`, `src/RunAction.cc`, `sim.cc`, plus new
`RunAccumulator`/`RunAccumulatorMessenger` source+header pairs and
`test/RunAccumulatorTest.cc`. `CLAUDE.md` needs a documentation update as
part of this change per the project's existing convention of documenting
every macro command and output file there.

## Token Usage

- **Input:** 50
- **Output:** 33593
- **Cache read:** 2032563
- **Cache creation:** 58959
- **Total:** 2125165
