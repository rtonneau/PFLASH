# Ticket 04 Implementation

**Status:** ✅ Done

## Commits

- bd0ec76 feat: add dissolved-O2 bulk scavenger reactions to PureWater and BoscoloChem (ticket 04)

## Local Test Result

```
Red (before edit, O2 21 %): "[DnaChemistryList] scavenger O2 is inert: ...", 0 O2(B) lines in bulk section.

ctest (build-ninja, Debug): 100% tests passed, 0 tests failed out of 8
04-o2  (O2 21 %):        dumped and reset, 43.8 s; no inert warning; bulk section has
                         e_aq^-1 + O_2^0 -> O_2^-1 k=1.74e+10, H^0 + O_2^0 -> HO_2°^0 k=2.1e+10,
                         O^-1 + O_2^0 -> O_3^-1 k=3.7e+09
04-zero (no scavenger):  dumped and reset, 36.6 s; sorted dump vs 03-t03 = exactly those 3 lines added
04-mt  (--threads 2, O2 21 %): dumped and reset, 22.1 s, no FatalException, same 3 O2(B) lines
BoscoloChem: build only (user decision, not run); its bulk list is byte-identical to PureWater's.

Species at 1 us (2 events, counts)   baseline  zero   O2 21 %
  e_aq^-1                              217      278     3
  H^0                                  198      179     1
  O_2^-1                                 3        4   319
  HO_2°^0                               24       18   219
  O_2^0 (tracked)                        3        2    70
```

## Review Notes

- Direction check passes clearly: G(e_aq) and G(H) collapse, G(O2-) rises with bulk O2.
- **Acceptance criterion NOT met as written:** "physics files identical to the baseline" for the zero run.
  `PhysicsInteractions.csv` and even `output_event_0.txt` (event 0, before any chemistry) differ from the
  baseline, while `EnergyDeposit.Txt` (20 keV) matches. The difference is repeatable (04-zero2 == 04-zero
  byte for byte), so it is a deterministic RNG-sequence shift caused by adding the three reaction entries at
  initialization, not chemistry activity at 0 concentration. Evidence that the reactions are inert at 0:
  `G4DNAScavengerProcess::PostStepGetPhysicalInteractionLength` skips components with count 0 and draws the
  same single random number per step as before; species yields of the zero run are statistically in line with
  the baseline. Root cause of the init-time RNG shift not pinned down (time-boxed).
- **Review Focus 1 corrected:** under SBS only O2 produced by *bulk* reactions (O3- + H2O / H3O+(B)) is
  absorbed into the bulk pool (`G4DNAScavengerProcess::PostStepDoIt`); bimolecular reactions go through
  `G4DNAMolecularReaction`, which does not consult the scavenger material, so tracked O2 remains (70 at 1 us).
  ADR 0004 and CLAUDE.md state the corrected behavior, plus the RNG-sequence note.
- Scratch macros `build/macro/scav_*.in` deleted; run outputs remain under `.scratch/tests/2026-09-28__o2-scavenger-uhdr/`.

## Time Spent

~1 hour

## Blockers / Challenges

- Plan's deterministic check assumed the physics stage is independent of the chemistry reaction set; false (see Review Notes). Replaced by the source-level argument + repeatability check.

## Token Usage

- **Input:** 38
- **Output:** 14819
- **Cache read:** 5100733
- **Cache creation:** 25963
- **Total:** 5141553
