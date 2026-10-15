# Ticket 05 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- 9905c62 feat: test_run.ps1 -RateAware/-MaxTimeStep/-ReactionBins; k-recovery check passes (ticket 05)

## Local Test Result

```
run: "done in 636 s" (build/, t05 run dir)

# k-recovery (ticket 05)

BoscoloChem, 0% O2, 100 keV, 2 events. Before = runs/o2_0, after = t05 (rate-aware + maxTimeStep 1 ns).

Bin = upper edge label in ns as written in Reactions.Txt.

| bin (ns) | OH+H2->H before | OH+H->none before | ratio before | OH+H2->H after | OH+H->none after | ratio after |
|---|---|---|---|---|---|---|
| 10 | 30 | 70 | 0.429 | 0 | 56 | 0.000 |
| 30 | 52 | 47 | 1.106 | 0 | 17 | 0.000 |
| 100 | 66 | 77 | 0.857 | 0 | 27 | 0.000 |
| 300 | 24 | 71 | 0.338 | 0 | 36 | 0.000 |
| 1000 | 43 | 71 | 0.606 | 0 | 38 | 0.000 |

Pass if ratio after < 0.05 in every bin (nan = no OH+H events and no OH+H2 events).

| reaction | before (10 ns-1 us) | after | factor |
|---|---|---|---|
| H^0 + OH^-1 -> e_aq^-1 | 124 | 0 | inf |
| °OH^0 + H2O2^0 -> HO_2°^0 | 134 | 1 | 134.00 |
| e_aq^-1 + H3O^1 -> H^0 | 1016 | 426 | 2.38 |

Pass: True

## e_aq + H3O -> H per bin

| bin (ns) | before | after |
|---|---|---|
| 10 | 240 | 160 |
| 30 | 184 | 41 |
| 100 | 253 | 76 |
| 300 | 196 | 78 |
| 1000 | 143 | 71 |

## Notes

- Bin labels are the edge values as written in Reactions.Txt (ns); the table includes label 10 through 1000, which covers either reading of the edge convention (superset of 10 ns-1 us). Pass holds for every row.
- Several after-values are 0 for OH + H2 -> H, so the ratio is 0.000; no bin has OH + H -> none = 0.
- Counts are from 2 events (low statistics, single run each; sim output is not reproducible run to run).

## Extra observation: e_aq + H3O -> H (not gated)

Totals over the bins above: before 1016, after 426 (factor 2.38). Per bin: 240->160, 184->41, 253->76, 196->78, 143->71 (see table above). The reaction drops mostly at late times (30 ns-1 us: factors 4.5, 3.3, 2.5, 2.0), less at the first bin. This is consistent with the exact formula as an observation only: for this pair R is about 0.16 nm, much smaller than sqrt(D*dt) at late times, so the old bridge would also over-accept it, and the reduction grows with dt.
```

## Review Notes

Default behavior of test_run.ps1 unchanged (extra macro lines only emitted when the new params are set). Full k-recovery.md and 05-krecovery.py are in .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/. Not rebuilt: script run from its tracked path.

## Time Spent

~0.4 hours (11 min simulation)

## Blockers / Challenges

None

## Token Usage

- **Input:** 32
- **Output:** 2242
- **Cache read:** 978679
- **Cache creation:** 84684
- **Total:** 1065637
