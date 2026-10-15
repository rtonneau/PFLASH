# Ticket 01: encounter-probability

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `header/chemistry/EncounterProbability.hh` declares, and `src/chemistry/EncounterProbability.cc` defines, `double EncounterProbability(double r1, double r2, double reactionRadius, double diffusionSum, double deltaTime)`, plain `double`s in any consistent units, standard library only, with no other project dependency (portable-class convention).
- [ ] It returns the exact radial formula from the plan's Strategy section; 1 if `r1 <= R` or `r2 <= R`; 0 if `R <= 0`, `deltaTime <= 0` or `diffusionSum <= 0`; and a result clamped to [0, 1]. When `r1·r2/(D·Δt)` > 700, `exp(-r1·r2/(DΔt))` underflows to 0 and the result equals P_G4; that must not produce NaN.
- [ ] `test/EncounterProbabilityTest.cc` (registered in `CMakeLists.txt` like `ChemistryRegistryTest`) asserts, in nm / ns units:
  - `EncounterProbability(2, 2, 0, 7, 1) == 0`
  - `EncounterProbability(0.4, 2, 0.5, 7, 1) == 1`
  - Geant4 limit (R·(r1+r2) >> D·Δt): r1 = r2 = 1, R = 0.5, D = 0.01, Δt = 1 -> within 1e-12 (relative) of exp(-(0.5·0.5)/0.01)
  - slow reaction: r1 = r2 = 2, R = 0.001, D = 7, Δt = 1 -> result < 0.01 while exp(-(1.999²)/7) > 0.5
  - monotonic: for R in {0.01, 0.1, 0.3, 0.6, 1.0} with r1 = r2 = 2, D = 7, Δt = 1, values strictly increase and stay in [0, 1]
  - `EncounterProbability(2, 2, 0.5, 7, 0) == 0`
- [ ] The test fails first (assertion failure, not a link error), then passes.

**Files to Touch:**
- `header/chemistry/EncounterProbability.hh`
- `src/chemistry/EncounterProbability.cc`
- `test/EncounterProbabilityTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
cmake --build build-ninja --target EncounterProbabilityTest && ctest --test-dir build-ninja --output-on-failure
```

Expected:
All tests `Passed` (including `EncounterProbabilityTest`), none `Not Run`.

**Notes:**

Use `std::expm1` for the denominator (`-std::expm1(-r1*r2/(D*dt))`) so it stays accurate when r1·r2 << D·Δt. The header comment states the formula, its derivation (method of images for u = r·p on the 3D radial process with an absorbing sphere at R) and that Geant4's `G4DNASmoluchowskiReactionModel` uses only the first exponential. Build through the MSVC env (`.claude/geant4-instructions.md` section 1).
