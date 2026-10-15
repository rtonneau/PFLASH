# Ticket 05: recipe-export-data-as-hdf5

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/export-data-as-hdf5.md` documents the generic pattern for writing per-event HDF5 output from a Geant4-DNA app: staging into a private pending directory during the run, then moving the finished file into the dump directory, verified against `src/scoring/MesoSpatialFile.cc` (not guessed).
- [ ] Cites the exact layout convention (`/run<R>/event<E>/snapshot<k>/`, datasets `position_nm`/`counts`, attributes `time_ns`/`cellSize_nm`) with a link to `docs/output/SpeciesMesoSpatial-h5.md` instead of repeating the full format spec.
- [ ] Includes a `## Serial vs MT` section verified against source: `MesoSpatialFile::AppendEvent` is guarded by a single process-wide `std::mutex` (`src/scoring/MesoSpatialFile.cc:22-25,139-144`) so concurrent worker threads serialize on the same file, in contrast to the `Run::Merge`-at-end-of-run pattern used by the counters in tickets 2-4 and 10 — state explicitly why HDF5 needs a different MT strategy (a shared file handle can't be split per-thread and merged after the fact).

**Files to Touch:**
- `docs/recipes/export-data-as-hdf5.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/export-data-as-hdf5.md && grep -q "mutex" docs/recipes/export-data-as-hdf5.md && grep -q "MesoSpatialFile" docs/recipes/export-data-as-hdf5.md && echo OK
```

Expected:
`OK`

**Notes:**

Read `src/scoring/MesoSpatialFile.cc` in full (it is marked portable/copy-paste, adjust-include-prefix-on-copy) — the recipe should be usable to add HDF5 export to a different kind of per-event data, not just restate this one file's purpose.
