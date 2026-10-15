# Ticket 01: sparse-capture-format-v2

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `MeshTotalsAction::TakeSnapshot` (`src/chemistry/TimeStepAction.cc`) appends a cell only when its species-column counts sum to > 0. A snapshot with no such cell has N = 0.
- [ ] `MesoSpatialFile`: `kFormatVersion = 2`; new `constexpr const char* kFormatDoc = "docs/output/SpeciesMesoSpatial-h5.md";`, written as the string root attribute `formatDoc` (UTF-8) when the file is created.
- [ ] `MesoSpatialFileTest` asserts `formatVersion == 2` and `formatDoc == kFormatDoc`. All ctest tests pass.
- [ ] Header and code comments that say "empty ones included" / "every cell" are updated.

**Files to Touch:**
- `src/chemistry/TimeStepAction.cc`
- `header/scoring/MesoSpatialFile.hh`
- `src/scoring/MesoSpatialFile.cc`
- `test/MesoSpatialFileTest.cc`

**Verification Step:**

Run (MSVC env as in `.claude/geant4-instructions.md`; then rebuild `sim` in `build/`):
```bash
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure && cmake --build build --config RelWithDebInfo --target sim
```

Expected:
`100% tests passed, 0 tests failed out of 13` and `sim` builds.

**Notes:**

Build each cell's row in a small local buffer, then append the row and the position only if the row sum is > 0. Keep the per-species `Column()` lookup as it is.
