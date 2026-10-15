# Ticket 03: sparse-smoke-validation

**Status:** ✅ Done

## Local Test Result

`check_meso_spatial.py .scratch/tests/2026-10-03__sparse-meso-spatial-output-and-format-doc/t03` (Serial, 1 event, 10 keV, endTime 1 ms; rerun in this session with the conda interpreter):
```
note: all-zero in h5 and absent from csv (consistent): O^0, O_3^-1, O_3^0
size: 3.36 MB total, 3.36 MB per event; v1 reference ~28 MB per event -> 8.3x smaller
1 event(s), 55 snapshot(s), 16 species; 1-event full comparison
OK
```

## Review Notes

Implemented by a sonnet subagent; reviewed in this session with no changes. The script adds checks for `formatVersion == 2`, a non-empty `formatDoc`, and no `counts` row with sum 0. The exact summed-count match with `SpeciesMeso.csv` still holds at all 55 times, so dropping empty cells loses no molecule. Size: 3.36 MB per event (v1 about 28 MB, 8.3x smaller).

## Blockers / Challenges

None.

## Commits

- 6a9fcb8 test(meso): validate sparse spatial snapshots (format v2) against SpeciesMeso (ticket 03)

## Time Spent

1m (ticket-start.js to ticket-complete.js)
