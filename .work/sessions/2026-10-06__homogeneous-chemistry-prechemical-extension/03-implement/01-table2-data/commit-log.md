# Ticket 01: table2-data

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja -R Tonneau2025TableTest --output-on-failure` (after building the target with the MSVC environment): `Test #14: Tonneau2025TableTest ... Passed`, 100% tests passed (1/1). Re-run by the session after the subagent's own run.

## Review Notes

Implemented by a sonnet/high subagent; reviewed in this session. 73 rows R1 to R73 present as std-only data; bulk flag on R1, R23, R58 to R73 as the ticket lists (R57 involves water but is not flagged, noted in the header). R20 to R37 were read from page images, because the PDF text extraction shifts the two-line rows (e.g. R22 is 8.00e9, R23 is 11); spot-checked against known literature values (H + H2O = 11, H + H = 7.52e9, H + H2O2 = 9.0e7). No changes after review. Open points listed in the header for ticket 4: whether k or 2k applies to like-molecule reactions (R1, R24, R34, R41, R46), R58 unit (taken as M/s), R43 and R56 species reading. `compile_commands.json` and the hook log changed from tooling and are not committed.

## Blockers / Challenges

`pdftoppm` is missing, so the Read tool could not render PDF pages; the subagent rendered page 7 with pymupdf installed into the scratchpad only.

## Commits

- 5893788 feat: Table 2 of Tonneau 2025 as plain reaction data with unit test (ticket 01)

## Time Spent

5m (ticket-start.js to ticket-complete.js)
