# Ticket 02: run-records

**Acceptance Criteria:**
- [ ] `RunAccumulator::AddRunRecord(const ManifestData::RunRecord&)` appends a record and does not set the pending-data flag.
- [ ] `RunAccumulator::GetRunRecords()` returns the records in insertion order.
- [ ] `ClearAccumulated()` empties the record list.
- [ ] `RunAccumulatorTest` covers add, order and clear, and all tests pass in `build-ninja/`.

**Files to Touch:**
- `header/scoring/RunAccumulator.hh`
- `src/scoring/RunAccumulator.cc`
- `test/RunAccumulatorTest.cc`

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja --output-on-failure
```

Expected:
Every test `Passed`, none `Not Run`.

**Notes:**

Plan Task 2. Existing `Accumulate(...)` signature stays unchanged so existing test calls keep compiling. `RunAccumulatorTest` needs no new source files: `ManifestData.hh` is header-only.
