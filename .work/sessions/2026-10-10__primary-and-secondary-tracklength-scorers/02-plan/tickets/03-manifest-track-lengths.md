# Ticket 03: manifest-track-lengths

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 0.66)
**Effort:** low

**Acceptance Criteria:**
- [ ] `RunManifest::Write` adds a `trackLengths` object built from `RunAccumulator`'s table summary: `events`, end-reason counts (`stopped`, `escaped`, `killed`), and for each numeric column `{ "mean": ..., "sem": ... }` keyed by the column name.
- [ ] With no rows, `trackLengths` has `events: 0` and no means (or nulls); the manifest still writes.
- [ ] The smoke run's `Manifest.json` shows `trackLengths.events == 2` and means that match the CSV.

**Files to Touch:**
- `src/scoring/RunManifest.cc`
- `CMakeLists.txt` (only if `TrackLengthTable.cc` must be added to the `sim` sources or a test target)

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo && ./sim <smoke-macro> --dir ../.scratch/tests/2026-10-10__primary-and-secondary-tracklength-scorers/manifest
```

Expected:
`tl/Manifest.json` contains `"trackLengths"` with `"events": 2`, the end-reason counts, and mean/sem for `primaryLength_nm` equal to the mean of the two CSV rows.

**Notes:**

Adding a manifest entry is one `Add` line in `RunManifest::Write` (ADR 0005); build the sub-node from `TrackLengthTable::Summarize()`. Keep key names identical to the CSV column names.
