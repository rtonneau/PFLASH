# Ticket 01: track-length-table

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.65)
**Effort:** medium

**Acceptance Criteria:**
- [ ] New portable class `TrackLengthTable` (standard library only, no Geant4 kernel or other dnachem-min dependency) in `header/scoring/TrackLengthTable.hh` / `src/scoring/TrackLengthTable.cc`.
- [ ] `Row` struct: `run`, `event`, `primaryLength_nm`, `primaryEkin0_keV`, `primaryEkinEnd_keV`, `primaryEnd` (enum Stopped/Escaped/Killed, written as `stopped`/`escaped`/`killed`), `secondaryFirstGen_nm`, `nSecondaryFirstGen`, `secondaryAll_nm`, `nSecondaryAll`.
- [ ] `Add(const Row&)`, `Merge(const TrackLengthTable&)` (argument unmodified), `Clear()`, `Size()`, `Empty()`.
- [ ] `WriteCsv(std::ostream&)` (header row + one row per event) and `WriteAscii(std::ostream&)` (`#`-commented header, whitespace-separated, like `PhysicsInteractionCounter`), both sorted by (run, event) whatever the insertion order.
- [ ] `Summarize()` returns the event count and, for each numeric column, mean and SEM (SEM = sample stddev / sqrt(n); 0 for n < 2; all zeros for n = 0), plus the count of each end reason.
- [ ] `test/TrackLengthTableTest.cc` (plain `assert`) covers add, merge order independence, clear, sorted CSV/ASCII output, end-reason strings, and summary mean/SEM on known values; registered in `CMakeLists.txt` like `PhysicsInteractionCounterTest`.

**Files to Touch:**
- `header/scoring/TrackLengthTable.hh`
- `src/scoring/TrackLengthTable.cc`
- `test/TrackLengthTableTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
cmake --build build-ninja --target TrackLengthTableTest && ctest --test-dir build-ninja -R TrackLengthTableTest --output-on-failure
```

Expected:
`100% tests passed, 0 tests failed out of 1`

**Notes:**

Follow `.claude/geant4-instructions.md` section 5 for the MSVC environment and the `NDEBUG` / Debug-CRT pitfalls. Match `PhysicsInteractionCounter.{hh,cc}` for header comments ("portable, adjust the include path prefix on copy") and stream-only I/O. Units in names: lengths nm, energies keV. Fixed-precision numeric output is fine; keep it consistent between Txt and csv.
