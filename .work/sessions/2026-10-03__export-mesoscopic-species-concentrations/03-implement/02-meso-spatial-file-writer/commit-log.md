# Ticket 02: meso-spatial-file-writer

**Status:** ✅ Done

## Local Test Result

`cmake -S . -B build-ninja && cmake --build build-ninja --target MesoSpatialFileTest && ctest --test-dir build-ninja -R MesoSpatialFileTest --output-on-failure` (MSVC env): `100% tests passed, 0 tests failed out of 1`.

## Review Notes

Implemented by a sonnet subagent; reviewed in this session with no changes.
- API, file name, staging paths and root attributes (`species`, `formatVersion` = 1, `units`) match the ticket.
- Layout `/run<R>/event<E>/snapshot<k>/`: `time_ns`/`cellSize_nm` attributes per group; `position_nm` (N×3 f64) and `counts` (N×S u32), chunked (≤4096 rows) with gzip 4. A shared snapshot hard-links both datasets, while each group keeps its own `time_ns`.
- Validation (sizes, snapshot index, species mismatch, duplicate event) runs before anything is written. A function-static mutex is held in the .cc, and the file is closed before returning.
- N = 0: the datasets are contiguous (HDF5 rejects zero chunk dims). Tested.
- The test uses its own `CHECK` macro (NDEBUG-safe) and compares objects with `H5Otoken_cmp` for the hard link (HDF5 1.14 has no addr). It writes under the system temp dir rather than the test working dir, which is harmless.
- Known limit, left for ticket 3/4: a staged file left by a crashed earlier process would be appended to by the next one (same as the pre-chemical staging folder).

## Blockers / Challenges

None.

## Commits

- 376afd4 feat(scoring): add MesoSpatialFile HDF5 writer for meso spatial snapshots (ticket 02)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
