# Ticket 02: scavenger-spec

**Acceptance Criteria:**
- [ ] `ScavengerSpec::Parse("O2 21 %")` gives molarity 0.21 × 0.0013; `mM`/`uM`/`M` convert by 1e-3/1e-6/1; `O2 0 %` is accepted as 0
- [ ] Parse rejects (false + message): negative value, unknown unit, non-numeric/non-finite value, wrong token count, `%` on a species other than `O2` (including wrong-case `o2`), pH-owned species `H2O`/`H2O(B)`/`H3Op(B)`/`OHm(B)`
- [ ] `Upsert` is last-wins and keeps the first position; `InertSpecies` returns species with molarity > 0 that are the partner of no bulk reaction, and skips zero entries
- [ ] `ScavengerSpecTest` is registered in CTest and passes; all other tests pass

**Files to Touch:**
- `header/geometry/ScavengerSpec.hh`
- `src/geometry/ScavengerSpec.cc`
- `test/ScavengerSpecTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
# inside the vcvars64 wrapper
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure
```

Expected:
`ScavengerSpecTest` Passed and every other test Passed, none Not Run.

**Notes:**

TDD: write the test and the CMake target first (full code in plan Task 2 Step 1), confirm the build fails on the missing header, then add the header and implementation (plan Task 2 Steps 3–4). Pure logic only: standard library + `chemistry/ChemistryTypes.hh`, molarity as a plain mol/L double, `constexpr double kO2SaturationMolarity = 0.0013`. Errors are returned as bool + string; callers raise `G4Exception`. The test `main()` starts with the MSVC CRT report-mode block. Deviation from the resume, agreed in the plan: pH-owned names are rejected here (at command time), not at `/run/initialize`.
