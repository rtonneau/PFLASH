# Ticket 04 Implementation

**Status:** ✅ Done

## Commits

- 76fa337 feat: add BoscoloChem chemistry (copy of PureWater) and example macro (ticket 04)

## Local Test Result

From `build` (RelWithDebInfo), scratch macros in `build/macro/`, outputs under `.scratch/tests/2026-09-28__multiple-dna-chemistry/`:

```
/chem/list:                       Available chemistries: PureWater (default) BoscoloChem
gps_conflict.in (PureWater then BoscoloChem):  fatal, exit=127, "already set to 'PureWater'" x1
gps_boscolo.in (/chem/select boscolochem):     exit=0  BOSCOLO_SAME  "chemistry = BoscoloChem" x1  failures 0
beam_boscolo.in:                                exit=0  success markers 2, "chemistry = BoscoloChem" x1
git diff --stat -- macro/beam.in:               (empty, unchanged)
```

`BOSCOLO_SAME`: sorted reaction dump of the `BoscoloChem` run equals the `PureWater` baseline dump (the copy is identical, as intended).

**Empty-list check** (`BuildBoscoloChemAcidBase()` temporarily `return {};`, files staged so `git restore` could undo it):

```
empty exit=0
failures (EEEE|FatalException): 0    warnings (WWWW): 0    success markers: 2
ReactionTable.txt: 67 lines (baseline) -> 47 lines; the "# Acid-base reactions" section header remains with no reactions
species at 1 ps plausible: °OH 968, e_aq 774, H3O+ 791, H 186
```

Result: installing `G4DNAScavengerMaterial` with no registered scavenger processes is safe. No gating needed and no ADR 0002 change. The temporary edit was reverted (`git restore`, `grep TEMPORARY` = 0), `sim` rebuilt, and the equality check re-run on the restored copy: `BOSCOLO_SAME_AGAIN`, 0 failures.

## Review Notes

- `sed` rename gave the expected names (`BuildBoscoloChemReactions` comes out of the general `PureWaterReactions` replacement). The only remaining "PureWater" hits are the deliberate WIP notice and a `ChemPureWaterBuilder` reference to the UHDR class.
- The WIP notice does not cite a paper or values; the header says the content is a copy of `PureWater`.
- The `sed`/Python edit in this session hit a tool quirk (a doubled backslash in an inline script was collapsed), so the header rewrite was redone with `chr(92)`. No effect on the committed files.
- Staged and committed only the four ticket files; `compile_commands.json` and the hook log were left out.

## Time Spent

~0.75 hours

## Blockers / Challenges

None.

## Token Usage

- **Input:** 22
- **Output:** 9018
- **Cache read:** 2598982
- **Cache creation:** 14647
- **Total:** 2622669
