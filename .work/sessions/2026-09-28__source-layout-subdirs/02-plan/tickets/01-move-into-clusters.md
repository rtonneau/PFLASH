# Ticket 01: move-into-clusters

**Acceptance Criteria:**
- [ ] Three baseline runs of `sim.exe` exist (`$S/base_a`, `base_b`, `base_c`, 10 events each, both scratch macros) and agree with each other within 15% on every quantity with a reference count of at least 400 (`node $S/compare.js base_a --ref base_b,base_c`). Amended: byte-identical output is not reproducible on this code.
- [ ] All 31 `.cc` and 33 `.hh` files sit in the cluster directories from the mapping; no `.cc` or `.hh` remains directly under `src/` or `header/`.
- [ ] `git diff --cached -M100% --name-status` reports 64 `R100` entries.
- [ ] The 7 test targets in `CMakeLists.txt` use the new `src/<cluster>/` paths; the include-directory loop is unchanged.
- [ ] Clean rebuilds of `build` (target `sim`) and `build-ninja` (7 test targets) succeed and `ctest` reports 7/7 Passed.
- [ ] `verify.sh after_moves` ends with `STATISTICALLY_COMPARABLE` (within 15% of the mean of the three baselines, quantities with reference count of at least 400).
- [ ] One commit: `refactor: cluster src/ and header/ into subdirectories (moves only)`.

**Files to Touch:**
- `src/**` and `header/**` (moves only, no content edits)
- `CMakeLists.txt` (the 7 test `add_executable` lines)
- `.scratch/tests/2026-09-28__source-layout-subdirs/` (macros, `verify.sh`, baselines; git-ignored)

**Verification Step:**

Run:
```bash
git diff --cached -M100% --name-status | grep -c '^R100'
ls src/*.cc header/*.hh 2>&1 | head -2
bash .scratch/tests/2026-09-28__source-layout-subdirs/verify.sh after_moves
ctest --test-dir build-ninja --output-on-failure
```

Expected:
`64`; then `ls` reports "No such file or directory" for both patterns; `verify.sh` ends with `STATISTICALLY_COMPARABLE`; ctest reports `100% tests passed, 0 tests failed out of 7`. (Run the two builds first, see Notes.)

**Notes:**

Do the steps in this order; the baseline must exist before any `git mv`.

1. **Pre-move sanity.** Build `sim` in `build` and the 7 targets in `build-ninja`, then `ctest --test-dir build-ninja --output-on-failure`. Expected 7/7 Passed. If not, stop: the tree was already broken.

2. **Scratch macros.** Create `$S/macros/slsd_water.in`:
   ```
   /run/verbose 0
   /tracking/verbose 0
   /dnaLogger/verbose Error
   /process/dna/e-SolvationSubType Ritchie1994
   /process/chem/TimeStepModel SBS
   /run/initialize
   /gun/particle e-
   /gun/energy 10 keV
   /run/beamOn 10
   /run/dumpDataAndReset
   ```
   `$S/macros/slsd_boscolo.in` is the same file with `/chem/select BoscoloChem` added before `/run/initialize`.

3. **Verify script and comparator.** `$S/verify.sh <label>` runs both scratch macros in parallel and, for non-baseline labels, runs `node $S/compare.js <label>`, which compares against the mean of `base_a`, `base_b`, `base_c` (physics-process totals, per-reaction totals keyed by label, species counts per time; energy deposit must be equal; tolerance 15%; only quantities with a reference count of at least 400). Both files live in the scratch dir. Amended after the two-run baseline turned out not to be reproducible (see commit-log); the user decided that statistical agreement within 10-15% is the requirement.

4. **Baselines.** Run `bash $S/verify.sh base_a`, `base_b` and `base_c` in the background (10 events at 10 keV; about 2.5 minutes per label). Calibrate with `node $S/compare.js base_a --ref base_b,base_c` (and the other two rotations): expected STATISTICALLY_COMPARABLE. Measured noise floor: worst deviation 12.6% at min count 400; below 400 counts, same-code runs already exceed 15%.

5. **Move the files** (repo root):
   ```bash
   set -euo pipefail
   move() { local dir=$1; shift; local n
     for n in "$@"; do
       if [ -f "src/$n.cc" ];    then mkdir -p "src/$dir";    git mv "src/$n.cc"    "src/$dir/$n.cc";    fi
       if [ -f "header/$n.hh" ]; then mkdir -p "header/$dir"; git mv "header/$n.hh" "header/$dir/$n.hh"; fi
     done; }
   move core     ArgParser OutputDir OutputDirMessenger DnaLogger DnaLoggerMessenger
   move actions  ActionInitialization PrimaryGeneratorAction RunAction Run EventAction TrackingAction StackingAction SteppingAction
   move geometry DetectorConstruction DnaChemistryWorld
   move physics  PhysicsList
   move chemistry DnaChemistryList TimeStepAction ChemUtils ScavengerReactionAccess ReactionTableDump ChemistryTypes ChemistryRegistry ChemistrySelectMessenger BuiltInChemistries
   move chemistry/catalog PureWaterReactions BoscoloChemReactions
   move scoring  ScoreSpecies PrimaryKiller ReactionCounter PhysicsInteractionCounter RunAccumulator RunAccumulatorMessenger
   ```
   `ChemistryTypes` and `ScavengerReactionAccess` are header-only; the `if` guards handle that.

6. **CMake test targets.** Replace the 7 `add_executable` lines in `CMakeLists.txt`:
   ```cmake
   add_executable(ArgParserTest test/ArgParserTest.cc src/core/ArgParser.cc)
   add_executable(OutputDirTest test/OutputDirTest.cc src/core/OutputDir.cc)
   add_executable(ReactionTableDumpTest test/ReactionTableDumpTest.cc src/chemistry/ReactionTableDump.cc src/core/DnaLogger.cc)
   add_executable(ReactionCounterTest test/ReactionCounterTest.cc src/scoring/ReactionCounter.cc src/core/OutputDir.cc)
   add_executable(PhysicsInteractionCounterTest test/PhysicsInteractionCounterTest.cc src/scoring/PhysicsInteractionCounter.cc)
   add_executable(RunAccumulatorTest test/RunAccumulatorTest.cc src/scoring/RunAccumulator.cc src/scoring/ReactionCounter.cc src/scoring/PhysicsInteractionCounter.cc src/core/OutputDir.cc)
   add_executable(ChemistryRegistryTest test/ChemistryRegistryTest.cc src/chemistry/ChemistryRegistry.cc)
   ```
   Leave the include-directory foreach loop alone: it keeps bare includes compiling in this commit.

7. **Verify and commit.** Clean-rebuild `build` and `build-ninja` (see the build snippet in the plan), run `ctest`, then `bash $S/verify.sh after_moves`. Commit with `git add -A src header CMakeLists.txt`.

`build/` holds a stale `ThreadsArgTest.exe`; ignore it.
