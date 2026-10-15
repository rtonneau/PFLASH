# Ticket 05: apply-fix-and-docs

**Status:** ✅ Done

## Local Test Result

`python docs/literature-comparison/farokhi2023/eaq_1ps_gap.py` prints Meesungnoen2002 4.150 (-0.6 % vs stock 4.176, inside the 1 % target) and Ritchie 4.036 (-3.3 %). `grep -h '^/process/dna/e-Solv' macro/beam_farokhi_*.in | sort | uniq -c` shows 41 x Meesungnoen2002.

## Review Notes

Done inline. All 40 Farokhi macros that set Ritchie1994 now set Meesungnoen2002 (one-line diff each; the meesungnoen test macro already did). Because ticket 3 was skipped, only the macro fix applies; no Chemistry code changed, so CLAUDE.md is unchanged. The comparison table was not regenerated (needs the DATA_SIM batches and notebook on the other machine); `simulation_vs_literature_comparison.md` now says its sim columns are Ritchie1994 and predate the change. The 1 % check uses the 250-event meesungnoen run from ticket 2; the macros themselves were not each re-run.

## Blockers / Challenges

None. Note: `beam_farokhi_electron_o2_0pct_meesungnoen.in` is now identical to `beam_farokhi_electron_o2_0pct_elossmin_2p5pct.in`.

## Commits

- 5ce5790 fix: use Meesungnoen2002 solvation in Farokhi macros to match the stock example (ticket 05)

## Time Spent

0m (ticket-start.js to ticket-complete.js)
