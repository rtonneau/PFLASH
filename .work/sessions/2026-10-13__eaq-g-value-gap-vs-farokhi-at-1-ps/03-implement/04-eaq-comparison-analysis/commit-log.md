# Ticket 04: eaq-comparison-analysis

**Status:** ✅ Done

## Local Test Result

`python docs/literature-comparison/farokhi2023/eaq_1ps_gap.py` wrote the PNG and markdown and printed G(1 ps): Ritchie 4.036 (-3.3 % vs stock), Meesungnoen 4.150 +/- 0.006 (-0.6 %), stock 4.176 +/- 0.006, Farokhi digitized 4.212 (+0.9 %).

## Review Notes

Done inline (no subagent). Script plots and tabulates the four sources and states the finding. Deviation from the ticket: no Farokhi-channels variant exists, because ticket 3 was skipped by the user's decision; the criterion about that variant is not met on purpose. Added data/meesungnoen_eaq.csv (the scratch run's e_aq curve) so the script runs from repo files only.

## Blockers / Challenges

None.

## Commits

- fb11638 docs: e_aq 1 ps gap analysis, solvation subtype explains it (ticket 04)

## Time Spent

1m (ticket-start.js to ticket-complete.js)
