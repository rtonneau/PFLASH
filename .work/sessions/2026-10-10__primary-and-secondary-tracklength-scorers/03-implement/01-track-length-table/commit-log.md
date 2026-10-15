# Ticket 01: track-length-table

**Status:** ✅ Done

## Local Test Result

`cmake --build build-ninja --target TrackLengthTableTest && ctest --test-dir build-ninja -R TrackLengthTableTest --output-on-failure` (MSVC env), re-run in the main session: `100% tests passed, 0 tests failed out of 1`.

## Review Notes

Implemented by a sonnet ticket subagent (medium effort). Reviewed in session against every criterion: std-only class, Row/End/Stat/Summary, Add/Merge/Clear/Size/Empty, sorted CSV/ASCII with identical fixed-6 formatting, sample-stddev SEM (0 for n<2, zeros for n=0), end-reason counts, test registered in CMakeLists.txt. No changes after review.

## Blockers / Challenges

None.

## Commits

- fb7cb1c feat: add portable TrackLengthTable per-event track-length table (ticket 01)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
