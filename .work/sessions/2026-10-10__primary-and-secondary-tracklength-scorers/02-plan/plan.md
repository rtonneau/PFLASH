# Implementation Plan

**Session:** primary-and-secondary-tracklength-scorers
**Date:** 2026-10-10T11:33:27.718Z
**Estimated effort:** 1 day

## Strategy

Build the kernel-free per-event table first, with its unit test. Then wire it through the Geant4 actions and the existing Run → RunAccumulator → Dump path, then add the manifest summary, then the docs. Each step builds and is verified on top of the previous commit.

## Tickets Overview

- **Ticket 1:** `TrackLengthTable`: portable per-event row table (add/merge/clear, sorted Txt/csv, mean ± SEM summary) plus a unit test.
- **Ticket 2:** Scoring pipeline: PrimaryKiller kill flag, TrackingAction collection, EventAction commit (aborted events skipped), Run/Merge, RunAccumulator, `TrackLengths.*` written at each Dump.
- **Ticket 3:** `Manifest.json` `trackLengths` summary.
- **Ticket 4:** Docs (CLAUDE.md, the sim-output skill, docs/output, the track_length_let note).

## Sequencing Rationale

Ticket 2 needs the table from ticket 1; ticket 3 needs the accumulated table from ticket 2; the docs describe the finished output.

## Risks & Mitigation

- **Risk:** MT merge drops or duplicates rows → **Mitigation:** compare Serial and `--threads 2` smoke runs (row count = events).
- **Risk:** end reason misclassified (solvation leaves KE at 25 meV) → **Mitigation:** PrimaryKiller flags its own kill; the smoke check expects `stopped` for 10 keV e-.
- **Risk:** Debug test binary vs Geant4 DLL mismatch → **Mitigation:** keep TrackLengthTable standard-library-only.

## Assumptions

- One gun primary per event (track 1); every other physical-stage track is a descendant e-.
- Build environment as in `.claude/geant4-instructions.md` (`build/` RelWithDebInfo for sim, `build-ninja/` Debug for ctest).
