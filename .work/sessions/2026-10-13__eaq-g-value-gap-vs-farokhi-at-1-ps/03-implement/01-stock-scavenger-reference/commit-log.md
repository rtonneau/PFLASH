# Ticket 01: stock-scavenger-reference

**Status:** ✅ Done

## Local Test Result

`head -5 docs/literature-comparison/farokhi2023/data/stock_scavenger_eaq.csv` gives the header `time_s,G_eaq_per_100eV,SEM,n_events` and rows from 1.0e-12 s with G = 4.176 (SEM 0.006, n = 250). The file has 31 time rows.

## Review Notes

Subagent: sonnet/medium. Checked against the acceptance criteria: the build is in the scratch dir, outside the repo; the run is 500 keV e-, eLossMin 12.5 keV, 250 events, seed 12345, 0 % O2; the CSV has 31 times with SEM; the note records the build and run commands and the macro. Nothing changed after review.
Finding for the later tickets: the stock run uses Meesungnoen2002 and gives 4.176 at 1 ps, 0.14 (3.4 %) above our 4.036 and close to the digitized Farokhi value of about 4.21. So the gap is real against the stock example. The stock run was not matched on solvation subtype, so its cause is still open.

## Blockers / Challenges

None. The example hardcodes eLossMax at 1 keV, so the run set `/primaryKiller/eLossMax 1 MeV` (our project default). The example writes only ROOT, so the subagent converted it with uproot installed in a scratch folder.

## Commits

- b514a8e docs: add stock scavenger e_aq reference run (ticket 01)

## Time Spent

9m (ticket-start.js to ticket-complete.js)
