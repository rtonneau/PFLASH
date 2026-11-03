# Session: primary-killer-energy-cuts-switch

**Date:** 2026-11-03T14:53:10.748Z
**Status:** Grill phase complete

## Problem Statement

The primary-electron energy cuts of `PrimaryKiller` (eLossMin kill, eLossMax abort, minKineticE kill) have no off switch. The Macro builder emulates "Off" by setting both loss thresholds to 10 MeV. Replace the emulation with an explicit `/primaryKiller/energyCuts <bool>` command, and make the dead `/primaryKiller/minKineticE` command work.

## Context & Constraints

- **Current behavior:** `PrimaryKiller::ProcessHits` applies the box cut to all tracks, then the energy cuts to track 1 (PDG 11) only; defaults 500 keV / 1 MeV from `DetectorConstruction`. `/primaryKiller/minKineticE` is registered with no `SetNewValue` branch and is never deleted. The builder writes `/primaryKiller/eLossMin 10 MeV` + `eLossMax 10 MeV` for Off (`KILLER_OFF`).
- **Pain point:** Off is an emulation that depends on the 1 MeV beam limit; minKineticE silently does nothing.
- **Dependencies:** Keep the box cut (`/primaryKiller/setSize`) unchanged; keep `gPrimaryKilled` semantics for `TrackLengths` end reasons; builder 'generated commands exist' test greps `PrimaryKiller.cc`.
- **Tech stack:** Geant4 11.4.1 C++ (G4UIcmdWithABool), explorer JS (`docs/explorer/macro-builder.js`, node tests, `build.js`), ctest in `build-ninja/`.

## Success Metrics

- `/primaryKiller/energyCuts false` with `/primaryKiller/verbose 1` (10 keV e-, beamOn 2, endTime 1 ms): no `kill track at` line, only `stopped`/`escaped` in `TrackLengths.csv`.
- 1 MeV e- run: primary track length with cuts off is larger than with cuts on (eLossMin 500 keV).
- `/primaryKiller/minKineticE` changes the threshold; builder Off writes `/primaryKiller/energyCuts false`; `node docs/explorer/macro-builder.test.js` and ctest (ExplorerCheck, MacroBuilderTest) pass.

## Architecture & Approach

- `PrimaryKiller`: new `G4UIcmdWithABool* fpEnergyCutsUI` → `/primaryKiller/energyCuts` (default true), member `G4bool fEnergyCuts = true`, `SetNewValue` branch, destructor delete, forward declaration, inline `SetEnergyCutsEnabled(G4bool)`. Early return becomes `if (!fEnergyCuts || trackID != 1 || PDG != 11) return FALSE;` (box cut stays first).
- `minKineticE`: add handler `fKineticE_Min = fpMinKineticE->GetNewDoubleValue(newValue)` and the missing `delete`; header comments updated. `energyCuts false` disables it too.
- Builder: Off writes `/primaryKiller/energyCuts false`; remove `KILLER_OFF` and its comment; update `killerMode` tech text (minKineticE "not offered", not "no effect"), the off test, README rule; rebuild with `node docs/explorer/build.js`.
- Docs: `CLAUDE.md` PrimaryKiller paragraph, `architecture.json` primary-killer entry if it lists commands.

## Assumptions & Trade-offs

- Plan item 4 needs no code change: per Geant4 source (`G4DNAOneStepThermalizationModel.hpp:184/244`, `G4ParticleChangeForGamma::InitializeForPostStep`), the thermalization stop leaves post-step KE > 0 (pre-step or 25 meV), so the `<= fKineticE_Min (0)` branch does not mislabel natural stops; the smoke run confirms it.
- Not doing: Manifest recording of cut state (PrimaryKiller lives on workers; would need a process-wide setting) — future work. Not offering minKineticE in the builder. No change to box cut, protons, secondaries, pulse structure.

## Open Questions

None.

## Notes

Source plan: `.claude/plans/2026-11-03-primary-killer-energy-cuts-switch.md`. PrimaryKiller needs the tracking kernel, so verification is by smoke run, not unit test.
