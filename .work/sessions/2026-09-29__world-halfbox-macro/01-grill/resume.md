# Session: world-halfbox-macro

**Date:** 2026-09-29T14:36:15.757Z
**Status:** Grill phase complete

## Problem Statement

`DnaChemistryWorld::fHalfBox` (default 500 um) can only be changed by recompiling. Provide a macro command to set it, and confirm that it defines the whole simulation geometry.

## Context & Constraints

- Confirmed: the world `G4Box` (`DetectorConstruction::ConstructDetector`) is built from `DnaChemistryWorld`'s `G4DNABoundingBox`; the world is the only volume. The same box is the chemistry diffusion boundary and the bulk-scavenger volume (`G4DNAScavengerMaterial` converts concentration to molecule count via `boundary->Volume()`; `G4DNAScavengerProcess` rate divides by `fpBoundingBox->Volume()`).
- `ConstructChemistryBoundary()` runs in the `DetectorConstruction` constructor, before macros; `G4DNAScavengerProcess` holds a reference to the box from `/run/initialize` on. Hence PreInit only, and the setter must rebuild the boundary.
- Bounded work: no plan/tickets, implemented on the checked-out branch (O2_Included).

## Success Metrics

- `/chem/env/halfBox 100 um` before `/run/initialize` gives a 200 um world, and `Manifest.json` shows `halfBox_um: 100`.
- The default run is unchanged (500 um).
- A value <= 0 is a fatal G4Exception at the command.

## Architecture & Approach

- `/chem/env/halfBox <value> <unit>`: `DeclareMethodWithUnit` on the existing `/chem/env/` G4GenericMessenger in `DnaChemistryWorld`, default unit um, `G4State_PreInit`.
- `SetHalfBox` validates (> 0, else fatal) and rebuilds `fpChemistryBoundary`.
- `ConstructDetector()` logs the world size (DnaLogger Info).
- `RunManifest::Write`: add `halfBox_um` next to `pH`.
- Docs: `DnaChemistryWorld.hh` UI header comment, CLAUDE.md Macro section.

## Assumptions & Trade-offs

- PreInit only: changing it between runs would need rebuilding the geometry, the scavenger processes' box reference and the scavenger counts. Not worth it.
- Half-side semantics, matching `fHalfBox`.
- No unit test: the validation is two lines in a Geant4-bound setter; verified by a sim.exe run.

## Open Questions

None.

## Notes

Verification: sim.exe, 10 keV e-, `/run/beamOn 2`, once with `/chem/env/halfBox 100 um` and once with the default.

## Token Usage

- **Input:** 36
- **Output:** 8314
- **Cache read:** 1078154
- **Cache creation:** 23411
- **Total:** 1109915
