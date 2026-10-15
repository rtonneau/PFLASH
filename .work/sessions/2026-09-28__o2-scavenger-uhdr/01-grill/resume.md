# Session: o2-scavenger-uhdr

**Date:** 2026-09-28T18:05:35.932Z
**Status:** Grill phase complete

## Problem Statement

`/chem/env/O2` is currently a no-op for the chemistry. `DnaChemistryWorld` already inserts an `O2` bulk component (when > 0 %) and `G4DNAScavengerMaterial` is always installed, but no `G4DNAScavengerProcess` reaction names `O2` as a bulk partner, so the dissolved-O2 background never reacts. Goal: implement dissolved O2 as a bulk scavenger exactly as the Geant4-DNA UHDR example does, and replace the O2-only command with a UHDR-style generic `/chem/env/scavenger` command.

## Context & Constraints

- UHDR (`EmDNAChemistry::ConstructProcess`) adds three bulk-O2 reactions into the same per-molecule `G4DNAScavengerProcess` as the acid-base ones: `e_aq + O2(B) -> O2-` (1.74e10 M^-1 s^-1), `H + O2(B) -> HO2°` (2.1e10), `O- + O2(B) -> O3-` (3.7e9). The bulk partner is the ordinary `O2` configuration (same as tracked O2).
- A molecule can carry only one `G4DNAScavengerProcess`; our acid-base list already has `H`, `e_aq`, `Om` entries — the O2(B) lines go into those entries.
- `G4DNAScavengerProcess` skips bulk components with count 0; `G4DNAScavengerMaterial::GetNumberMoleculePerVolumeUnitForMaterialConf` returns 0 for components absent from its table -> registering at 0 % is inert and safe.
- Bulk O2 is consumed as it reacts (H3O+/OH-/H2O are not); `G4Scheduler::Process` resets the scavenger material at the start of every event's chemistry by default (`fResetScavenger = true`).
- `PureWater`'s bimolecular reactions against *tracked* radiolytic O2 (`e_aq/H/O- + O2`) stay unchanged.
- Molecule table is empty at PreInit -> species names from the command can only be validated in `ConstructChemistryComponents` (during `/run/initialize`).
- Project rules: SBS only, no UHDR pulse features, single water box; DnaLogger for diagnostics; unit tests are pure logic only (no Geant4 kernel), built/run in `build-ninja/`.

## Success Metrics

- `/chem/reaction/dump` output lists the three O2(B) bulk reactions (on e_aq, H, Om).
- Smoke run (10 keV e-, `/run/beamOn 2`): `beam_o2.in` (O2 21 %) vs `beam.in` (0 %) — G(e_aq) and G(H) at 1 µs clearly lower and G(O2-) higher with O2 (direction check only, 2 events).
- 0 % run yields unchanged vs current behaviour.
- `ctest` in `build-ninja/`: all tests pass, including new `ScavengerSpecTest`.

## Architecture & Approach

1. **Reactions per Chemistry (same as UHDR):** `PureWater` and `BoscoloChem` each add the three O2(B) reactions to their per-molecule bulk list (UHDR rates). Always registered; inert at 0 concentration.
2. **Rename** acid-base -> bulk reaction throughout: `ChemistryTypes::AcidBaseList/Entry/Reaction` -> `BulkReactionList/Entry/Reaction`, `buildAcidBase` -> `buildBulkReactions`, `BuildPureWaterAcidBase` -> `BuildPureWaterBulkReactions` (and Boscolo equivalent), `RegisterAcidBaseScavengerProcesses` -> `RegisterBulkReactionProcesses`, reaction-dump section header.
3. **Command** `/chem/env/scavenger <species> <value> <unit>` on `DnaChemistryWorld` (PreInit): units `M`, `mM`, `uM`, `%` (`%` only for O2, kH = 0.0013 M; else fatal); negative value or unknown unit fatal; repeated species -> last wins; 0 accepted = absent. `/chem/env/O2` removed; `IsOxygenScavengerEnabled`/`SetOxygenPercent`/`GetOxygenConcentration` dropped; `beam_o2.in` migrated to `/chem/env/scavenger O2 21 %`.
4. **Validation in `ConstructChemistryComponents`:** species not in molecule table -> fatal `G4Exception`; pH-owned species (`H2O`, `H3Op(B)`, `OHm(B)`) -> fatal; species not used as partner by any bulk reaction of the selected Chemistry -> `DnaLogger` Warning ("inert scavenger").
5. **Pure-logic helper** (e.g. `ScavengerSpec` in `src/chemistry/` or `src/geometry/`): parse species/value/unit and convert to molarity incl. `%` rule and errors (bool + message, messenger/world raises `G4Exception`); covered by `test/ScavengerSpecTest.cc`.
6. **Docs:** CLAUDE.md (macro/command docs, key files), `DnaChemistryWorld.hh` header comment, CONTEXT.md (done during grill: Scavenger, Bulk species, Bulk reaction), new ADR `docs/adr/0004-scavenger-reactions-per-chemistry.md` + "superseded in part" note in ADR 0001.

## Assumptions & Trade-offs

- Chose per-Chemistry scavenger reactions (UHDR layout) over a shared scavenger layer: rates belong to the network (ADR 0002), and one-process-per-molecule forces merging anyway. Concentration mechanism stays environment-wide.
- Chose generic `/chem/env/scavenger` over keeping `/chem/env/O2`: breaking change is harmless since `/chem/env/O2` never had an effect.
- Always-registered O2(B) reactions: simpler wiring, relies on verified zero-count skip in `G4DNAScavengerProcess`.
- Bulk-O2 consumption per event is negligible for the 1 mm water box, and reset per event anyway.
- Smoke-run acceptance is direction-only (2 events), not quantitative G-value agreement.

## Open Questions

- ADR 0004: recommended in the grill (Q12), not explicitly confirmed by the user before `/gps write` — included in the docs scope; confirm at planning.

## Notes

Out of scope: counting/reporting bulk reactions in `ReactionCounter`; scavengers other than O2 (syntax allows them, they will warn as inert); CO2/NO2-/HCO3- molecules; UHDR pulse structure. Grill decisions: Q1 (a) UHDR three reactions; Q2 same as UHDR -> per-Chemistry (Q7 rename confirmed); Q3 (a) always registered; Q4 (b) generic command; Q5 no counting; Q6 acceptance all; Q8 (a) remove `/chem/env/O2`; Q9 validation all three; Q10 syntax ok; Q11 unit test ok.

## Token Usage

- **Input:** 46
- **Output:** 18492
- **Cache read:** 1936132
- **Cache creation:** 132660
- **Total:** 2087330
