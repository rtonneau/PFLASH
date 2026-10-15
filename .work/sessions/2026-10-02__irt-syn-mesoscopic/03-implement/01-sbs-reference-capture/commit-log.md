# Ticket 01 Implementation

**Status:** ✅ Done

## Commits

- `4b93fce` test(validation): SBS reference dumps and validate macros (ticket 01)

## Local Test Result

```
Subagent (SBS build, --threads 10):
  validate_water.in N=40   109 s   rel. err e_aq 2.56 %, OH 2.22 %, H2O2 2.46 %, H2 1.57 %
  validate_o2.in    N=1500 5227 s  rel. err e_aq 2.26 %, OH 0.31 %, H2O2 0.32 %, H2 0.36 %
Inline re-check:
  recomputed both references from sumG/sumG2 at t=999.999 ns: identical to the READMEs
  re-ran validate_water.in --threads 10: exit 0, 123.8 s, G(1 us) e_aq 1.255 (ref 1.245),
  OH 1.608 (1.597), H2 0.689 (0.713), H2O2 0.431 (0.429), all within ~1.4 sigma
```

## Review Notes

- Implemented by a subagent on **sonnet**. The inline follow-up reviewed the macros and READMEs, recomputed the errors independently, and re-ran the water macro. It did not re-run the 87-min O2 run, but checked that its CSV is byte-identical to the scratch output. **No changes.**
- 10 threads were used instead of 4, which the criteria allow (MT allowed). The READMEs record it.
- In the O2 case e_aq at 1 µs is about one molecule per event (G ≈ 0.013), which drove N = 1500. Ticket 07's 10% check on e_aq with O2 needs similarly high statistics.
- The macros also set `/process/dna/e-SolvationSubType Ritchie1994` (as `beam.in` does), and the O2 macro sets pH 7.
- The sim target had nothing to relink, so the macros were copied into `build/macro` by hand. Later tickets must rebuild or copy after macro changes.

## Time Spent

~1.7 hours (mostly the unattended O2 run)

## Blockers / Challenges

None.

## Token Usage

- **Input:** 118
- **Output:** 4949
- **Cache read:** 5544131
- **Cache creation:** 468602
- **Total:** 6017800
