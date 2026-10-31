# sim.cc readability pass

Standing approval covers these changes, after geometry verification/commit.

## Clarity

- sim.cc:67-239: main mixes CLI registration, configuration, runtime setup,
  macro execution and cleanup. Pure-move flag registration into
  RegisterArgumentFlags(ArgParser&) at the same call point; pure-move the
  already-scoped lazy-root block into ConfigureDefaultOutputDir.
- Preserve parsing/returns, object lifetimes, initialization and cleanup order.
  Propose broader setup/batch helpers but defer ownership/early-return changes.
- sim.cc:157,174: remove stale commented TOML/visualization commands.
- The unused global output stream and local storage string remain: removing
  objects would also remove their initialization/destruction effects.

## Comments

- File header: macro-driven homogeneous radiolysis; Geant4/project/OS dependencies,
  master setup and run-manager shutdown expectations.
- sim.cc:56-64,77-84: remove overstated exact reproducibility; correct wall-time
  and Quiet/Error startup descriptions against the actual calls.
- sim.cc:115-117,132-133: explain lazy output-root fallback and that a macro
  worker-count override requires an MT run manager already selected by CLI.
- Remove comments merely repeating obvious GUI/setup/cleanup statements;
  retain ordering and output-flush rationale.

## Naming

- sim.cc:38-48: ec/buf/n/exe -> error/executablePathBuffer/pathLength/
  executablePath. No system API calls, capacities or error checks change.
- sim.cc:52,79,87,138,192: useGUI -> kUseGui; theTimer/UIManager/available/t
  -> simulationTimer/uiManager/availableCores/candidatePath.
- CLI flags, macro argument position, log text and serialized fields unchanged.

## Documentation

- CLAUDE.md:35: update the internal GUI constant name. GUI remains disabled
  and is not exercised in this pass.
- Static helpers get purpose/precondition/failure notes as applicable;
  this translation unit exposes no reusable public header API.

## Deferred behavior issues

- Macro remains argv[1]; ignored ApplyCommand return can hide command failure.
- Missing-macro early return bypasses manual cleanup.
- GUI branch references missing macro/vis.mac; not repaired or enabled.
- OS lookup capacity/portability and unused stream/storage lifetime are retained.

## Verification required

Debug build/17 ctest with debug HDF5 PATH, RelWithDebInfo build, frozen
63-invocation/252-event matrix against original 1fcf8ba. Token audit reverses
renames and inlines both helpers, restoring the old lazy-block braces. Record
actual results and files/categories/risk before committing this concern.

## Verification results

- Debug and RelWithDebInfo builds passed; ctest 17/17 passed in 1.20 s,
  with vcpkg debug/bin first on PATH. Token audit and git diff --check pass.
- The two extracted helpers inline back to the old tokens and lazy-block
  scope exactly after reversing the local names. No C++ edits after the builds.
- The frozen 63-invocation/252-event matrix passed against original baseline
  1fcf8ba. All invocations returned 0, printed completion/dump markers, and
  matched output structures/time grids. Only baseline WrongResolution occurred.
- Comparison: 229 scalar observables, 3,310 descriptive curve points; zero
  flagged differences at Holm-adjusted alpha 0.01. Minimum raw p =
  0.0168135776; minimum adjusted p = 1.0. Twenty invocation-level replicates
  (80 events) per mode per side; no detected regression at this sample size,
  not a proof of identical distributions or rare-channel equivalence.

| H2O2 endpoint | Baseline mean | Sim mean | Difference 99% CI |
|---|---:|---:|---|
| SBS, molecules/100 eV | 0.521750 | 0.532750 | [-0.017186, 0.039186] |
| IRT, molecules/100 eV | 0.770250 | 0.764250 | [-0.048730, 0.036730] |
| Mesoscopic, molecules/event | 82.7875 | 82.9875 | [-3.140696, 3.540696] |

TrackLengths.csv and PhysicsInteractions.csv were exactly equal for 2/42 SBS,
12/42 IRT and 29/42 mesoscopic dump pairs each; all numerical observables are
included in the comparison regardless of exact file equality.

Commands: verify_builds.ps1 -Phase sim; audit_sim.py e6ce062; run_matrix.py sim;
compare.py sim; summarize_matrix.py sim; git diff --check. Build/ctest logs,
token audit and simulation data are in .scratch/readability-2026-10-29/.
GUI and production macros were not run.

## Directory deliverable

Files: sim.cc, its kUseGui reference in CLAUDE.md, and this plan. Changes are
two scoped pure-move helpers, local naming and purpose/dependency/thread/order
comments. The helper call boundaries are the main structural risk; token audits
restore the original statements and lazy-block scope. Deferred issues above
remain unchanged. No CLI flag, output filename, manifest key, default, physics
value, RNG call, runtime initialization or cleanup order was changed.
