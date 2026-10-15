# Ticket 07 Implementation

**Status:** ✅ Done

## Commits

- `e0a5b88` fix(chem): merge tracked scavenger-species molecules into the bulk pool at hand-over (O2 hang)
- `baf4b29` fix(scoring): record SpeciesMeso after every Gillespie step (stale late records)
- `94f5197` docs(validation): IRT_syn + mesoscopic validation report, ADR 0006 accepted (ticket 07)

## Local Test Result

```
Inline re-check from the dumps:
  Serial vs --threads 4 (5 blocks x 40 events each, post-fix build), --tol 0.0332:
    e_aq -1.80%, OH -1.70%, H2O2 +3.04%, H2 +1.02%  -> max 3.04% (tol 3.3%): PASS, exit 0
  vs SBS (documented, exit 1 expected):
    water (N=40):  e_aq +25.01%, OH +21.16%, H2O2 +51.14%, H2 -1.93%
    O2 (N=1500):   e_aq +9.00%,  OH -1.70%,  H2O2 +36.88%, H2 +30.93%
Fix verification (inline):
  O2 hang: validate_o2.in 1500 events MT x10 completes (1218 s, 1500/1500 chemistry ends, 0 EEEE)
  SpeciesMeso: recorded total at 1000 ns = true mesh total (25796 = 25796); e_aq decays at k[O2] to 1 us
Subagent: 1 s runs, N=40, MT x10: water 469.7 s, O2 2135.1 s, exit 0, no EEEE
```

## Review Notes

- **Attempt 1 BLOCKED:** water failed the 10% SBS gate, and the O2 run hung on one thread. Inline diagnosis:
  - D1–D3 runs showed that most of the gap is IRT_syn vs SBS itself.
  - The user decided IRT_syn is the model of record, with the SBS difference documented rather than gated.
- **Attempt 2 BLOCKED:** the O2 hang was located in the particle stage, in an infinite loop in `G4DNAIndependentReactionTimeStepper::FindReaction`. Inline root cause:
  - `Voxelizing` leaves radiolytic O2 tracks alive (seen in 271/300 events).
  - Fixed in `e0a5b88`.
- **Inline: O2 e_aq +159% vs SBS.** The cause was `G4DNAEventScheduler`'s reaction-only `RecordTime` leaving stale late records; fixed in `baf4b29`, after which e_aq is +9%.
- **Attempt 3 READY:** a subagent on **sonnet** wrote the report and ADR and ran Serial vs MT and the 1 s runs. The inline follow-up re-ran the gate comparison and the SBS comparisons from the dumps and checked the citations (only arXiv 2409.11993, as the method reference). It also fixed one stale ADR line (the initial cell is 15.26 nm on the default box after the cap).
- **Open, documented:**
  - the water mesoscopic surplus (+9 points e_aq, +14 °OH over IRT_syn alone);
  - the IRT_syn vs SBS H2O2/H2 difference;
  - a late H2O2 rise in the 1 s O2 run;
  - the Serial vs MT H2O2 difference, at 3.6 standard errors on 5 blocks.

## Time Spent

~4 hours (three attempts, inline diagnosis and two fixes, long runs)

## Blockers / Challenges

Resolved: the O2 hang (`e0a5b88`) and the stale `SpeciesMeso` records (`baf4b29`). The physics differences against SBS are documented as open questions, per the user's decision.

## Token Usage

- **Input:** 548
- **Output:** 87414
- **Cache read:** 63979269
- **Cache creation:** 476714
- **Total:** 64543945
