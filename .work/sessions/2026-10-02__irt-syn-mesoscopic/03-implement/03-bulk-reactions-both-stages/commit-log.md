# Ticket 03 Implementation

**Status:** ✅ Done

## Commits

- `f50eb2c` feat(chem): bulk reactions in both stages, diffusion class in catalogs (ticket 03)

## Local Test Result

```
Inline re-run (--threads 2, 10 keV x 2, end time 1 ms):
  build exit 0; ctest 10/10 passed
  03-o2:    exit 0, dumped and reset, 0 EEEE, conservation OK (both workers),
            meso bulk O_2^0 net -203 / -245 per event, (B) in Reactions.Txt = 0, duplicated species lines = 0
  03-water: exit 0, dumped and reset, 0 EEEE, conservation OK, (B) = 0, dups = 0,
            H3O^1 780 @ 1 ps -> 778 @ 10 ps (buffered, no decay to 0)
Subagent (Serial): O_2^-1 = 15 in the hand-over row; H3O^1 783 -> 608 at hand-over (SBS reference ratio 0.80 vs 0.78)
```

## Review Notes

- Implemented by a subagent on **opus**. The inline follow-up reviewed the catalog diff (rates and products unchanged, only reaction types changed, each group cites option3 or UHDR) and re-ran both macros with `--threads 2`. **No changes.**
- **Accepted decision:** the bulk pairs that already exist as tracked-O2 entries (e_aq+O2, H+O2, O⁻+O2) share that table entry instead of being added twice. Adding them again would double the Gillespie propensity. A mismatch in rate or products between the two definitions is a fatal error. UHDR also has a single table entry per pair.
- **Diffusion class:** PureWater has 34 reactions at type 1 and BoscoloChem 21 of 26 (option3 Type II/IV). The O2 pairs follow option3 (type 1), whereas UHDR leaves them at 0.
- **New `WrongResolution` warning,** once per event, from `G4DNAEventScheduler::InitializeInMesh`. It comes from the effective radii of the D = 0 bulk partners, which Gillespie doesn't use. It is benign, and ticket 06 or 07 should document it.
- **Performance:** with bulk reactions active in Gillespie, the mesoscopic stage takes 28–46 s per event to 1 ms. Ticket 07's runs to 1 s will be long.
- `TimeStepAction.cc` was touched outside the ticket's file list, for a Debug-only bulk-consumption log line. Accepted.

## Time Spent

~0.4 hours

## Blockers / Challenges

None.

## Token Usage

- **Input:** 132
- **Output:** 5667
- **Cache read:** 10865003
- **Cache creation:** 188957
- **Total:** 11059759
