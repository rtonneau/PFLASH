# Implementation Plan

**Session:** macro-dump-and-reset
**Date:** 2026-09-24T06:57:23.155Z
**Estimated effort:** 1 day

## Strategy

Build the feature bottom-up in three commits: (1) give `OutputDir` a
filename prefix — a small, self-contained change every later ticket
depends on; (2) add `RunAccumulator`, the pure-logic, unit-testable
persistent store for energy/reaction/interaction data; (3) wire it all
together with `RunAccumulatorMessenger` (the `/run/dumpDataAndReset`
command + the exit-time safety net), trim `RunAction::EndOfRunAction`
down to a single `Accumulate()` call, and bring the project's own
docs/tooling config back in sync with the new behavior. Each ticket
leaves the tree buildable and (for 01/02) testable on its own.

## Tickets Overview

1. `OutputDir` gains a filename prefix (`SetPrefix`/`Resolve`) — TDD, unit
   tested in `test/OutputDirTest.cc`.
2. `RunAccumulator` — process-wide, cross-run accumulator for energy +
   the two counters, plus the prefix-uniqueness guard and pending-data
   flag. Pure logic, no G4 kernel dependency — TDD, unit tested in new
   `test/RunAccumulatorTest.cc`.
3. `RunAccumulatorMessenger` — the actual `/run/dumpDataAndReset [prefix]`
   command (species lookup + file writing + reset), the exit-time safety
   net, `RunAction`/`sim.cc` wiring, and the matching doc/config updates
   (`CLAUDE.md`, `.claude/.claude-project.json`,
   `.claude/geant4-instructions.md`). No unit test (matches this
   project's convention: messengers are never unit-tested); verified by
   building and running `sim.exe` against a scratch macro.

## Sequencing Rationale

01 has no dependency on anything new and is the extension point every
later output-writing call already uses. 02 depends on nothing from 01
functionally, but is sequenced second because 03 needs both: it calls
`OutputDir::SetPrefix` (01) and `RunAccumulator`'s accessors (02) in the
same function. 03 must be last since it's the only ticket that changes
externally-observable behavior (removes the automatic per-run
write/clear) and is where the docs need to describe the *finished*
behavior, not an intermediate state.

## Risks & Mitigation

- **Behavior change for existing macros**: any macro relying on the old
  automatic per-`beamOn` write/clear (e.g. multiple `beamOn`s expecting
  separate `_bis`-suffixed output) now needs an explicit
  `/run/dumpDataAndReset` or will fall back to the single exit-time
  `EndOfRun_`-prefixed flush. Mitigated by documenting this clearly in
  `CLAUDE.md` and `.claude/geant4-instructions.md` (ticket 03) — no
  existing macro is silently broken (data is never lost, just
  potentially bundled into one `EndOfRun_` output instead of several).
- **Linking a G4-kernel-dependent symbol into a test binary would crash
  it at runtime** (calling `G4SDManager`/`ScoreSpecies` methods without a
  live kernel null-derefs). Mitigated by keeping all species-scorer
  lookup and file-writing code inside `RunAccumulatorMessenger.cc`
  (never linked into any test target) and keeping `RunAccumulator.cc`
  itself free of any such dependency, so `RunAccumulatorTest.cc` only
  ever calls the pure-logic subset.
- **Forgetting to update the project's own build/verification config**
  (`.claude/.claude-project.json`'s `run.successMarkers` /
  `run.benignWarnings`, `.claude/geant4-instructions.md`'s `_bis` note)
  would leave future verification passes checking for console lines that
  no longer appear. Covered explicitly as the last step of ticket 03.

## Assumptions

- Carried over from the grill resume unchanged: prefix has no
  auto-inserted separator; the prefix-uniqueness guard is in-memory only
  (not checked against the filesystem) and resets every process
  restart; `RunAccumulator` is project-specific, not designed to be
  copy-paste portable.
- The exit-time safety net always uses the literal prefix `EndOfRun_`
  (no numbering/timestamping) and bypasses the uniqueness guard, so it
  can never itself fail.

## Token Usage

- **Input:** 60
- **Output:** 71519
- **Cache read:** 4751847
- **Cache creation:** 112629
- **Total:** 4936055
