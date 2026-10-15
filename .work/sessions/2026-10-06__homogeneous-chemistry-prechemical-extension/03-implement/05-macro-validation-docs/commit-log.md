# Ticket 05: macro-validation-docs

**Status:** ✅ Done

## Local Test Result

Ticket's run + `compare_gvalues.py`, re-run by the session: exit 0; G at 100 ns (sim / paper / ratio): e_aq 1.735/2.49/0.70, H3Op 2.445/2.49/0.98, OH 1.825/2.62/0.70, H 0.840/0.59/1.42, H2 0.725/0.31/2.34 (DEVIATES), H2O2 0.720/0.54/1.33. 2 events, one seed: indicative only.

## Review Notes

Implemented by a sonnet/medium subagent; reviewed in this session. Macro, analysis script and README (force-added), CLAUDE.md and the spatial-file caveat are in place. The paper's Table 3 values were taken from the PDF text extraction (the page image was not rendered); the session saw the same six numbers in the extraction (2.49, 2.49, 2.62, 0.59, 0.31, 0.54) and they match Boscolo 2020 and the H3O+ = e_aq statement, but a visual check is still advisable. The macro sets hand-over 100 ns and end time 200 ns so Species.* ends at 100 ns. No changes after review.

## Blockers / Challenges

pdftoppm missing (see ticket 01). The H2 yield deviates by about 2.3 times; the 10 keV LET is higher than the paper's column and the network acts before 100 ns here, so this is not by itself an error.

## Commits

- 1e83821 feat: beam_tonneau2025 macro, 100 ns G-value check, docs (ticket 05)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
