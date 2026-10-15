# Session: pflash-housecleaning-port-from-dnachem-min

**Date:** 2026-10-15T16:23:33.749Z
**Status:** Grill phase complete

## Problem Statement

The repo is a fresh copy of the Geant4-DNA project "dnachem-min", now named PFLASH (PFAS + FLASH). The aim of PFLASH is to implement the dose-rate effect. Before that development starts, the repo must read coherently as PFLASH: no dnachem-min working history, no stale names, scope text that states the PFLASH aim. This session is housecleaning only; no behavior change.

## Context & Constraints

- **Current behavior:** executable `sim` (sim.cc), CMake project `Simulation`, docs/comments/skills/tests say `dnachem-min`; ~400 tracked files under `.work/` (old sessions, retro, reads), `.claude/plans/*`, `.kb-notes/`, a stray `report` file, `docs/cloud-work/`; CLAUDE.md scope says UHDR is a use case and dose-rate/pulse structure must not be added; ~45 Farokhi2023 macros flat in `macro/`.
- **Pain point:** names and history from the old project make the port incoherent and mislead future work.
- **Dependencies:** keep all four Chemistries (PureWater, BoscoloChem, Tonneau2025, Farokhi2023) and their validation data; keep ADRs, CHANGELOG entries, the 2026-09-29 review and literature-comparison write-ups verbatim (history); current gps session must survive.
- **Tech stack:** Geant4 11.4.1 DNA, CMake/Ninja, MSVC, HDF5, C++20; build/ (RelWithDebInfo, run) and build-ninja/ (Debug, ctest).

## Success Metrics

- Clean build of `pflash` in `build/` and all ctest tests pass in `build-ninja/`.
- Smoke run (10 keV e-, `/run/beamOn 2`) produces the same outputs as before the rename.
- Grep for `dnachem-min`, `sim.exe`, `sim.cc` and the `sim-output` skill name finds only the kept historical records (ADRs, CHANGELOG, 2026-09-29 review, literature-comparison write-ups) and the origin note.
- CLAUDE.md, README and CONTEXT.md state the PFLASH purpose consistently.

## Architecture & Approach

Remove: old gps sessions (via gps `clean.js`, keeping the current one), `.work/retro`, `.work/reads`, `.work/scratch`, `.work/changelog`, `.claude/plans/*`, `.claude/plan-class-based-chemistry.md`, `.kb-notes/` (and `projectNotesPath` from `.claude-project.json`), hook log, `report`, `docs/cloud-work/`.
Rename: executable/target `sim` to `pflash`, `sim.cc` to `pflash.cc`, CMake `project()` and the `Simulation` target, skill `sim-output` to `pflash-output`, `dnachem-min-macro-command-reference.md` to `pflash-macro-command-reference.md`; replace `dnachem-min` in living docs, header/source comments, diagrams, `.claude-project.json`, `geant4-instructions.md`, tests.
Restructure: move the Farokhi2023 macros into `macro/farokhi2023/`; verify the `macro/<arg>` lookup in the main source and the build's macro copy handle the subfolder (adjust docs/paths).
Rewrite text: CLAUDE.md Purpose and Scope, README, CONTEXT.md state the PFLASH aim (PFAS + FLASH, dose-rate effect as next development); the "do not add dose-rate/pulse structure" rule becomes "not in this housecleaning; next development". Add an origin note ("derived from dnachem-min") in README and CLAUDE.md.
Keep, rename mentions only: analysis/, validation/reference/, the irt-syn-meso PNGs.

## Assumptions & Trade-offs

- No physics/chemistry/output behavior changes; no pulse or dose-rate code now.
- ADRs, CHANGELOG, old review and literature write-ups keep `dnachem-min` text as history (not rewritten).
- Removed history stays recoverable in git history and the original dnachem-min repo.
- Old sessions are deleted with gps `clean.js`, never by hand.

## Open Questions

- Whether the build-ninja/build dirs and user's MSVC scripts outside the repo reference `sim.exe` (not visible from the repo).

## Notes

Remote origin is already git@github.com:rtonneau/PFLASH.git. Next task after this session is new development (dose-rate effect), planned separately.
