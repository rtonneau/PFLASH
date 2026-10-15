# Ticket 01: sparse-capture-format-v2

**Status:** ✅ Done

## Local Test Result

`cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` (rerun in this session, MSVC env): `100% tests passed, 0 tests failed out of 13`. The subagent also built `sim` (RelWithDebInfo) cleanly. The simulation output is checked in ticket 03.

## Review Notes

Implemented by a sonnet subagent; reviewed in this session with no changes. `TakeSnapshot` fills a per-cell row buffer and appends the row and position only when the row sum is > 0, so N can be 0. `kFormatVersion` = 2. `kFormatDoc` is written as the UTF-8 root attribute `formatDoc` on file creation. The test checks both. Comments are updated. The commit also carries the `CONTEXT.md` glossary change from the grill (Spatial snapshot = cells holding at least one molecule).

## Blockers / Challenges

None.

## Commits

- 4a3234e feat(meso): write only non-empty cells, spatial format v2 with formatDoc (ticket 01)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
