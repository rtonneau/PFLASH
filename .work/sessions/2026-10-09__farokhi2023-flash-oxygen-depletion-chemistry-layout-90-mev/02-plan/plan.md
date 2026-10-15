# Implementation Plan

**Session:** Farokhi2023 FLASH oxygen-depletion chemistry layout (90 MeV proton Fig2/Fig3, 0.5 MeV electron Fig5)
**Date:** 2026-10-09T16:32:29.326Z
**Estimated effort:** 1-2 days

## Strategy

Land the two project-wide, cross-cutting edits first (physics constructor, species time grid) in isolation so later tickets build against their final state. Then add the new `Farokhi2023` Chemistry (pure catalog addition, no existing-file edits besides registration). Then validate and document the pre-existing but dormant `PrimaryKiller` class, since the electron macros depend on it. Then add the 8 example macros (proton batch first, since it needs no `PrimaryKiller` work; electron batch second). Finish with the comparison/analysis script, which needs real output from the macros to be meaningful.

## Tickets Overview

- **Ticket 1:** Switch `PhysicsList.cc` from `G4EmDNAPhysics` to `G4EmDNAPhysics_option2`, keeping the old constructor call commented out.
- **Ticket 2:** Extend `ScoreSpecies.cc`'s hardcoded time-recording grid to ~3-4 log-spaced points per decade between 1 ps and 1 µs.
- **Ticket 3:** Add the `Farokhi2023` Chemistry (9 base reactions + 3 O2-scavenger bulk reactions, no acid-base buffer) and register it.
- **Ticket 4:** Validate the existing dormant `PrimaryKiller` class against the current chemistry pipeline and document it in CLAUDE.md.
- **Ticket 5:** Add the 4 proton example macros (90 MeV, O2 = 0/3/7/21%).
- **Ticket 6:** Add the 4 electron example macros (0.5 MeV, O2 = 0/2.5/5/21%, track-segment truncation via `PrimaryKiller`).
- **Ticket 7:** Add a session-local comparison/plotting script against digitized Fig. 2/3/5 reference points.

## Sequencing Rationale

Tickets 1-2 are independent, project-wide, and have no dependency on the new Chemistry, so they're safest to land and verify first in isolation. Ticket 3 (the new Chemistry) has no dependency on 1-2 but must land before any macro can select it. Ticket 4 (`PrimaryKiller`) is independent of 1-3 but is a hard dependency for ticket 6 (electron macros use it); it's sequenced after 3 only so the validation smoke-run can use the final `Farokhi2023` Chemistry rather than an unrelated one. Ticket 5 depends on ticket 3 only. Ticket 6 depends on tickets 3 and 4. Ticket 7 depends on 5 and 6 having real output to plot against.

## Risks & Mitigation

- **Risk:** Switching to `G4EmDNAPhysics_option2` shifts numeric output for every existing Chemistry/macro in the project. → **Mitigation:** No unit test pins physics-simulation numbers (confirmed during the grill); the old constructor stays commented out in the file for one-line revert if needed.
- **Risk:** `PrimaryKiller`'s hardcoded defaults in `DetectorConstruction.cc` (`SetMinLossEnergyLimit(500*keV)`, `SetMaxLossEnergyLimit(1*MeV)`) are already live for every macro today, not just newly activated — a macro with an electron beam depositing more than 500 keV cumulative would already be silently truncated before this session. → **Mitigation:** Ticket 4 documents this as existing (not new) behavior; no existing example macro's beam energy is anywhere near that threshold (confirmed: all use 10-100 keV), so no observable regression, but the implementer must not assume `PrimaryKiller` is inert.
- **Risk:** The paper's Table 2 reaction rates were initially misread (OCR artifacts: `[H2]` for `[O2]`, `HH2` for `HO2`) and could be mismatched against the project's existing `PureWater` rates, which looked similar but are NOT the same values for 7 of the 9 base reactions (`PureWaterReactions.cc` uses its own "project values", not the paper's). → **Mitigation:** Ticket 3's Notes pin every rate against `examples/extended/medical/dna/scavenger/Rtable_O2.txt` in the Geant4 11.4.1 source tree (ground truth, not the PDF's OCR), confirmed during the grill.

## Assumptions

- No test pins Geant4-DNA physics-simulation numeric output, so ticket 1's physics-constructor switch cannot break `ctest`.
- `O-`/`O3-` molecule configurations already exist in the shared molecule table (used today by `PureWater`/`Tonneau2025`); ticket 3 needs no new molecule construction.
- `/primaryKiller/*` UI commands are not state-restricted (confirmed: `macro/beam_o2.in` and `macro/beam.in` already show them, commented out, placed after `/run/initialize`), so ticket 6's macros may set them anywhere before `/run/beamOn`.
- Event counts in tickets 5/6 (`/run/beamOn 20` for protons, `/run/beamOn 50` for electrons) are starting points for a visible trend, not publication-grade statistics; anyone rerunning the macros can raise them.
