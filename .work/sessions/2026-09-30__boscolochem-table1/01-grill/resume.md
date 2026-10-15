# Session: boscolochem-table1

**Date:** 2026-09-30T11:11:07.912Z
**Status:** Grill phase complete

## Problem Statement

The `BoscoloChem` Chemistry is still a work-in-progress verbatim copy of `PureWater`. It must reproduce exactly Table 1 of Boscolo et al., "Impact of Target Oxygenation on the Chemical Track Evolution of Ion and Electron Radiation", Int. J. Mol. Sci. 2020, 21, 424 (26 reactions, k in 10^10 dm3 mol-1 s-1).

## Context & Constraints

- Files: `src/chemistry/catalog/BoscoloChemReactions.cc`, `header/chemistry/catalog/BoscoloChemReactions.hh`; CLAUDE.md's BoscoloChem entry.
- Molecules, dissociation channels and diffusion coefficients are shared by all Chemistries (ADR 0002) -> Table 2 (D values) is out of scope.
- ADR 0002: a Chemistry may omit the acid-base buffer if its header says so.
- Bounded work: no plan/tickets, no branch; lands on `O2_Included`.

## Success Metrics

- Reaction table under `/chem/select BoscoloChem` contains exactly the 26 Table 1 reactions with Table 1 rates (checked via `/chem/reaction/dump` against a transcribed Table 1 in the session scratch dir).
- Bulk-reaction list = only e_aq + O2(bulk) -> O2- (1.9e10) and H + O2(bulk) -> HO2 (2.0e10).
- `build/` and `build-ninja/` build; ctest green; `beam_boscolo.in` runs.

## Architecture & Approach

- Tracked-pair reactions: the 26 rows (i)-(xxvi), H2O reactants/products omitted (untracked), rates = table value x 1e10 M^-1 s^-1, default reaction type.
- (xiv)/(xv) apply both to tracked radiolytic O2 (reaction table) and to bulk O2 (bulk-reaction list), same rates.
- Drop the acid-base buffer entirely; drop O- + O2 (tracked and bulk) and every O-/O3-/H2O2+OH- reaction not in Table 1.
- Headers: remove WIP marker, cite the paper, state buffer absent by design (ADR 0002), `/chem/env/pH` has no chemical effect under BoscoloChem, Table 2 D not applied.

## Assumptions & Trade-offs

- Same-species rates (i), (vi), (x), (xxv) taken verbatim (they match Geant4's k, not 2k, convention).
- O- and O3- remain in the shared molecule set but are inert under BoscoloChem.
- Verification is dump + manual diff, no new unit test (catalog builders need the Geant4 kernel).

## Open Questions

None.

## Notes

Table 1 transcribed from a rendering of PDF page 13 (text layer is garbled): `.scratch/tests/2026-09-30__boscolochem-table1/table1.png`.

## Token Usage

- **Input:** 36
- **Output:** 9761
- **Cache read:** 1055862
- **Cache creation:** 28030
- **Total:** 1093689
