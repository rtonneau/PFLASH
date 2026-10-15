# Implementation Plan

**Session:** Verify feature combinations and add three example macros with standard dir tree
**Date:** 2026-10-08T18:56:37.820Z
**Estimated effort:** 2-3 days (mostly run time for the regression)

## Strategy

Add the mode machinery first (SBS registration, meso switch), then the things that depend on it (end-time default, manifest keys), then the two independent I/O features (macro lookup, results index), then examples and docs, then verification. Each ticket builds and passes ctest on its own.

## Tickets Overview

- **Ticket 1:** SBS selectable via `/process/chem/TimeStepModel`; `/chem/meso/enable` switch; `TimeStepAction` skips the hand-over when meso is off.
- **Ticket 2:** End-time default (1 us without meso, 1 s with); manifest records the mode; no `SpeciesMeso*` files when meso is off.
- **Ticket 3:** Portable `MacroResolver` with unit test; `sim.cc` uses it.
- **Ticket 4:** Default output dir `<exedir>/results`; `results/Manifest.json` index of dumps, with unit test.
- **Ticket 5:** `example_sbs.in`, `example_irt.in`, `example_meso.in`; ADR 0008; CLAUDE.md, README and sim-output skill updates.
- **Ticket 6:** Smoke matrix: 3 modes x {O2, no O2} x {Serial, 4 threads}; fix whatever interferes.
- **Ticket 7:** SBS regression against `validation/reference/sbs_{water,o2}`; short report in `docs/`.

## Sequencing Rationale

1 before 2 (end time and manifest depend on the mode setting). 3 and 4 are independent of 1-2 but 5 needs all of them (the examples use the lookup, the results layout and the modes). 6 and 7 need the finished tree; 7 is last because it is the longest run.

## Risks & Mitigation

- **Risk:** Code written for IRT_syn (StackingAction, ReactionCounter, TimeStepAction user-reaction hooks, molecule counter) breaks under SBS. → **Mitigation:** compare with the SBS-era code at `b26a262^` and run a short SBS case at the end of ticket 1.
- **Risk:** Windows Debug-CRT dialogs or the `NDEBUG` pitfall in unit tests. → **Mitigation:** follow `.claude/geant4-instructions.md` section 5.
- **Risk:** O2 SBS regression is slow. → **Mitigation:** N=300, MT, run in the background.

## Assumptions

- Mesoscopic stage after SBS is out of scope; meso=true with SBS is fatal.
- Existing macros and default behavior stay unchanged.
- MSVC environment and build procedure as in `.claude/geant4-instructions.md`.
