# Implementation Plan

**Session:** primary-killer-energy-cuts-switch
**Date:** 2026-11-03T14:56:23.075Z
**Estimated effort:** 2 hours

## Strategy

First the C++ command (and the minKineticE handler), verified by smoke runs; then the Macro builder and docs switch from the 10 MeV emulation to the new command.

## Tickets Overview

- **Ticket 1:** `/primaryKiller/energyCuts <bool>` in PrimaryKiller, plus working `/primaryKiller/minKineticE` (handler + delete).
- **Ticket 2:** Builder Off writes `/primaryKiller/energyCuts false`; KILLER_OFF removed; tests, README, CLAUDE.md, architecture.json updated; explorer rebuilt.

## Sequencing Rationale

The builder's 'generated commands exist' test greps PrimaryKiller.cc for the command, so the C++ must land first.

## Risks & Mitigation

- **Risk:** natural primary stops mislabelled `killed` → **Mitigation:** source shows post-step KE > 0 on thermalization; confirm with the smoke run's TrackLengths.csv.
- **Risk:** stale explorer build → **Mitigation:** run `node docs/explorer/build.js` and the ExplorerCheck ctest.

## Assumptions

- `build/` (RelWithDebInfo) and `build-ninja/` (Debug) are configured as in `.claude/geant4-instructions.md`.
