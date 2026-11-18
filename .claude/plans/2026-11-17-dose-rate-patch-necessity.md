**Branch:** test/dose-rate-patch-necessity
**Estimated effort:** 2-3 days (plus ~3 h of background Geant4 kernel builds)

## Strategy

Kernels first, because every check runs against them and they take hours to build: split the rebound fold into its own patch and write the kernel build script, then start the seven builds in the background. While they build, create the suite skeleton and the three sufficiency fixtures, cheapest first (rebound fold, then finder, then IRT_syn source contacts). Each fixture is verified on `stock` and its `p<N>` variant as soon as that kernel exists. Next, make `dose-rate-study` build on a kernel without patch 2, run the leave-one-out necessity histories, and finish with the driver and the report docs.

## Tickets Overview

- **Ticket 1:** Add `rebound-fold.patch` (the `calculateNextCoordinate` hunk only) and a kernel build script that creates, verifies and reuses the 7 kernel variants under `.scratch/kernels/`.
- **Ticket 2:** Standalone `test/dose-rate-patches/` CMake project with the shared check/expectation harness, plus fixture 3 (the rebound fold overshoot table), green on `stock` (defect) and `p3` (fixed).
- **Ticket 3:** Fixture 1 (3 coincident reaction products in the octree finder under a Job Object memory cap), green on `stock` (cap hit) and `p1` (built within a few MiB).
- **Ticket 4:** Fixture 2 (a delayed track at contact with an older molecule under IRT_syn, with `RegisterSourceActivation` detected by CMake), green on `stock` (0 reactions at activation) and `p2` (about N×P).
- **Ticket 5:** `dose-rate-study` detects `RegisterSourceActivation` in CMake; without it `SourceInjector` skips registration and the output records `sourceActivation=false`. Production behaviour is unchanged.
- **Ticket 6:** Necessity runs: `dose-rate-study` built on `all` and on each `all-<N>`, histories at 1e4 Gy/s, 0.1 Gy, 10 realizations, metric extraction (memory cap or abort, new-old contacts at activation, molecules outside the box) and the report-only G(beam-off) delta.
- **Ticket 7:** Top-level driver that writes the generated report into `.scratch`, plus `docs/dose-rate-pulse-study/patch-necessity.md` linked from issues 1-3 in `geant4-issues.md`.

## Sequencing Rationale

Ticket 1 blocks everything, since every check needs the kernels. Ticket 2 sets up the harness the other fixtures reuse, and its fixture needs no kernel internals, so it proves the per-kernel build plumbing cheaply. Tickets 3 and 4 then add one fixture each. Ticket 5 must land before ticket 6, because `all-2` cannot link `dose-rate-study` without it. Ticket 7 aggregates the results of tickets 2-6.

## Risks & Mitigation

- **Risk:** A Geant4 build fails or is slow (7 × ~26 min, several GB each). → **Mitigation:** Build in the background with logs, one variant at a time, and make the script idempotent so it resumes. Check free disk space before starting, and build `p3`/`p1` first so the fixtures can start.
- **Risk:** The stock finder fixture exhausts host memory. → **Mitigation:** Run it suspended in a Job Object with a 256 MiB cap and kill-on-close (the `JobLauncher.cc` pattern) and a deadline.
- **Risk:** Fixture 2 is non-deterministic, so stock and patched are not clearly separated. → **Mitigation:** Pick a high-probability pair from the `CalculateStep` source and a large enough N, and assert stock == 0 exactly and patched within a binomial bound.
- **Risk:** A history on `all-1` does not meet a coincident packet, so the defect is not triggered. → **Mitigation:** Choose the packet range to include a capture-library packet known to hold ≥3 coincident molecules, verified on `all` first.
- **Risk:** Particle snapshots miss out-of-box molecules on `all-3`. → **Mitigation:** Add a final-state position check, or count folds that land outside the box.
- **Risk:** A `dose-rate-study` change alters production results. → **Mitigation:** Detection is compile-time only. Its existing ctests and a same-config smoke run on `all` must match the behaviour before the change.

## Assumptions

- The shared Geant4 11.4.3 source and install stay untouched. Patches go only into private copies, applied with `git -c core.autocrlf=false apply`.
- The MSVC x64 environment and vcpkg toolchain follow `.claude/geant4-instructions.md`. Suite and `dose-rate-study` builds are Release/RelWithDebInfo against Release kernels.
- The three patches touch disjoint files, so any combination applies cleanly.
- `stock` is the shared install at `${G4_ROOT}`.

--- ticket: 01-kernel-variants ---

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `patches/geant4/11.4.3/rebound-fold/rebound-fold.patch` holds only the `calculateNextCoordinate` rewrite (no probe counters, no scheduler hunks) and applies cleanly to pristine 11.4.3.
- [ ] The build script defines the variants `p1`, `p2`, `p3`, `all`, `all-1`, `all-2`, `all-3` with their patch sets. For each, `git apply --check` succeeds on pristine source.
- [ ] For each variant the script copies the pristine source (excluding `.git`), applies the patches, configures/builds/installs into `.scratch/kernels/<variant>/{source,build,install}`, and writes a `kernel.json` recording the patches, the SHA-256 of the patch-owned files and the `G4processes_core.dll` hash.
- [ ] A rerun skips variants that are already built and verified. `-Verify` checks that the source files equal pristine + patches and that the DLL hash matches.
- [ ] All 7 variants are built and verified (background builds started in this ticket).

**Files to Touch:**
- `patches/geant4/11.4.3/rebound-fold/rebound-fold.patch`
- `patches/geant4/11.4.3/rebound-fold/README.md`
- `test/dose-rate-patches/BuildKernels.ps1`
- `test/dose-rate-patches/kernels.json`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/BuildKernels.ps1 -Verify
```

Expected:
One line per variant `p1 p2 p3 all all-1 all-2 all-3: OK (patches applied, DLL hash matches)`, exit code 0.

**Notes:**

Follow the configure options of the shared install (see its `lib/cmake/Geant4` config and `docs/dose-rate-pulse-study/reproduction.md`). Use `git -c core.autocrlf=false apply` with absolute patch paths. Build variants one at a time in the background (`p3`, `p1`, `p2` first) and wait on them with `t3_background_wait`. Check free disk space first. Never touch `${DEV_DIR}/GEANT4/geant4-v11.4.3-source` or `${G4_ROOT}`. Derive paths from `.claude/.claude-project.json` + `DEV_DIR`/`G4_ROOT`, never hardcoded.

--- ticket: 02-suite-and-rebound-fixture ---

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `test/dose-rate-patches/` is a standalone CMake project (`find_package(Geant4 11.4.3 EXACT)`) that configures against any kernel variant into its own build dir, records the kernel variant it was built on, and registers ctests.
- [ ] A shared header provides explicit checks (not `assert`) and an `--expect defect|fixed` mode, and prints measured values in a machine-readable line (`RESULT key=value ...`).
- [ ] Fixture 3 calls `G4ChemReboundTransportation::calculateNextCoordinate` on [0,1] for 2.1, 2.9, 1.0 and on [-2,2] µm for 7 µm, plus a dose-rate scenario (random steps overshooting a 4 µm reflecting box, all results must lie inside it).
- [ ] On `stock` with `--expect defect`: it reproduces 1.7, -0.7, 0.0, 3 µm and finds out-of-box results. Exit 0.
- [ ] On `p3` with `--expect fixed`: it gives 0.1, 0.9, 1.0, -1 µm and nothing outside the box. Exit 0.

**Files to Touch:**
- `test/dose-rate-patches/CMakeLists.txt`
- `test/dose-rate-patches/PatchCheck.hh`
- `test/dose-rate-patches/ReboundFoldFixture.cc`
- `test/dose-rate-patches/README.md`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/RunSufficiency.ps1 -Patch 3
```

Expected:
`rebound-fold: stock defect observed (2.1->1.7, 2.9->-0.7, 1.0->0.0, 7um->3um); p3 fixed (0.1, 0.9, 1.0, -1um) -> PASS`, exit 0.

**Notes:**

`calculateNextCoordinate` is a public static, so it needs no scheduler. Expected values are in `geant4-issues.md` §3 and in the reverted `examples/poisson-track-irt/test/ReflectingFoldTest.cc` (commit `e925b0d`). Put the bin dir of the kernel under test first on `PATH` and log the loaded module paths, so a run can never silently pick up the wrong DLL. `RunSufficiency.ps1` (a thin per-patch runner, extended in tickets 3-4) configures/builds the suite for `stock` and `p<N>` and runs the fixture on each.

--- ticket: 03-finder-fixture ---

**Model:** heavy
**Effort:** high

**Acceptance Criteria:**
- [ ] Fixture 1 inserts 3 coincident molecules (the products of e_aq + e_aq → 2 OH⁻ + H₂, at one point) plus ordinary neighbours into the octree finder and queries their neighbours.
- [ ] It runs inside a Job Object (256 MiB committed-memory cap, kill-on-close, deadline). The launcher reports cap hit, exit code and peak memory.
- [ ] On `stock` with `--expect defect`: the cap is hit (or the tree blows up). The launcher turns that into PASS.
- [ ] On `p1` with `--expect fixed`: the tree builds, peak memory is within a few MiB, the neighbour query returns the coincident molecules, exit 0.

**Files to Touch:**
- `test/dose-rate-patches/FinderCoincidentFixture.cc`
- `test/dose-rate-patches/JobLauncher.cc`
- `test/dose-rate-patches/CMakeLists.txt`
- `test/dose-rate-patches/RunSufficiency.ps1`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/RunSufficiency.ps1 -Patch 1
```

Expected:
`finder-coincident: stock defect observed (memory cap 256 MiB hit); p1 fixed (peak < 32 MiB, 3 coincident neighbours found) -> PASS`, exit 0.

**Notes:**

Reuse `test/molecular-finder/FinderOracle.cc` (the minimal coincident case) and `JobLauncher.cc` (Job Object pattern); details in `docs/dose-rate/molecular-finder.md`. The octree fix is in `.icc` headers, so the fixture must compile against each kernel's own include dir. The finder singleton lives in the DLL (issue 12), so go through the public `G4OctreeFinder` API.

--- ticket: 04-source-contact-fixture ---

**Model:** heavy
**Effort:** high

**Acceptance Criteria:**
- [ ] CMake detects `G4DNAIndependentReactionTimeStepper::RegisterSourceActivation` (`check_cxx_source_compiles`) and defines `HAS_SOURCE_ACTIVATION` 0/1.
- [ ] Fixture 2 sets up an IRT_syn chemistry with an old molecule present from t = 0 and a new molecule injected as a delayed track at t_a > 0 within the effective reaction radius of the old one (a new-old contact). When the API is present, the fixture registers the source exactly as `SourceInjector` does.
- [ ] Over N independent trials it counts reactions between the pair at t_a, and records the pair probability P.
- [ ] On `stock` with `--expect defect`: 0 reactions at activation. Exit 0.
- [ ] On `p2` with `--expect fixed`: the count is within a binomial 4-sigma band of N×P and > 0. Old-old contacts behave as on `stock`. Exit 0.

**Files to Touch:**
- `test/dose-rate-patches/SourceContactFixture.cc`
- `test/dose-rate-patches/CMakeLists.txt`
- `test/dose-rate-patches/RunSufficiency.ps1`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/RunSufficiency.ps1 -Patch 2
```

Expected:
`source-contact: stock defect observed (0/N reactions at activation); p2 fixed (k/N, expected N*P +- 4 sigma) -> PASS`, exit 0.

**Notes:**

Read `G4DNAIndependentReactionTimeStepper::CalculateStep` (lines ~160-180) and `Prepare()`/`InitializeReactions` before picking the pair: choose one with a large contact probability (e.g. e_aq + OH) and an N that separates 0 from N×P. Reuse the scheduler/delayed-track setup in `test/timed-injection/probe/` (e.g. `InjectionReference.cc`, `FinderFixture.cc`) and `examples/dose-rate-study/src/SourceInjector.cc`. The fixture may need a minimal G4 kernel (run manager, chemistry list). Keep it Serial. The first-packet double trial is not a criterion.

--- ticket: 05-drs-activation-detect ---

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `examples/dose-rate-study/CMakeLists.txt` detects `RegisterSourceActivation` and defines `DRS_HAS_SOURCE_ACTIVATION` 0/1.
- [ ] Without the API, `SourceInjector` skips `ResetSourceActivations`/`RegisterSourceActivation`, logs it once, and history output records `sourceActivation: false` (`true` otherwise).
- [ ] `dose-rate-study` configures and builds against `stock` and against `all`.
- [ ] Existing `dose-rate-study` ctests pass on `all`, and a short history on `all` gives the same result as before the change (same config and seeds, same binary type).

**Files to Touch:**
- `examples/dose-rate-study/CMakeLists.txt`
- `examples/dose-rate-study/src/SourceInjector.cc`
- `examples/dose-rate-study/include/SourceInjector.hh`
- `examples/dose-rate-study/src/HistoryMain.cc` (or wherever the history ledger/manifest is written)
- `examples/dose-rate-study/README.md`

**Verification Step:**

Run:
```bash
ctest --test-dir <drs-build-all> --output-on-failure && cmake --build <drs-build-stock>
```

Expected:
All `dose-rate-study` tests pass on `all`, the `stock` build succeeds, and the history output has `sourceActivation: true` on `all` and `false` on `stock`.

**Notes:**

The check is compile-time only. Don't change any runtime path when the API exists. Issue 15 (rebuilt binaries diverge) means "same result" is compared on the ledger summary: arrivals and end time exact, chemistry within statistical noise, or a same-binary repeat.

--- ticket: 06-necessity-runs ---

**Model:** heavy
**Effort:** high

**Acceptance Criteria:**
- [ ] A runner builds `dose-rate-study` against `all`, `all-1`, `all-2`, `all-3`. It makes one capture library on `all` and reuses it everywhere.
- [ ] Histories: 1e4 Gy/s, 0.1 Gy, `halfBoxUm = 2`, 10 realizations with the same chemistry/source seeds on every variant, run under memory and wall caps. The packet range includes a packet with ≥3 coincident molecules (found and documented).
- [ ] Patch 1: `all-1` histories hit the memory cap or abort, and `all` completes every history.
- [ ] Patch 2: new-old contact reactions at activation (from the per-track reaction log) = 0 on `all-2` and > 0 on `all`.
- [ ] Patch 3: molecules outside the box (snapshots and final positions) > 0 on `all-3` and 0 on `all`.
- [ ] Report-only: G(beam-off) delta per species (mean, SE, z) for `all` vs `all-2` and `all` vs `all-3`.
- [ ] Results are written as JSON for the report.

**Files to Touch:**
- `test/dose-rate-patches/RunNecessity.ps1`
- `test/dose-rate-patches/necessity/necessity.json`
- `test/dose-rate-patches/necessity/analyze_necessity.py`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/RunNecessity.ps1 -Reuse
```

Expected:
`patch1: all-1 aborted 10/10 (memory cap), all completed 10/10 -> PASS; patch2: new-old contacts all-2 0, all >0 -> PASS; patch3: out-of-box all-3 >0, all 0 -> PASS`, plus a G-value delta table. Exit 0.

**Notes:**

Reuse `examples/dose-rate-study/studies/` (`make_configs.py`, `run_study.ps1`, `analyze.py`), the per-track reaction log (`ReactionLog`, new-old contacts at activation) and particle snapshots (`scripts/snapshots.py`). If snapshots at 0.1 Gy don't catch out-of-box molecules, add a final-state position check or count folds that land outside the box. If `all-1` does not reliably abort, report it per realization rather than faking a pass. Run histories in parallel in the background.

--- ticket: 07-driver-and-report ---

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `test/dose-rate-patches/RunAll.ps1` builds or reuses the kernels, runs sufficiency checks 1-3 and the necessity checks, and writes `.scratch/tests/<session>/patch-necessity-report.md` (plus JSON) with per-patch side-by-side values and verdicts. Exit 0 only when every demonstration holds.
- [ ] `docs/dose-rate-pulse-study/patch-necessity.md` summarises the results (method, kernel variants, tables, the G-value delta, caveats). Issues 1-3 in `docs/dose-rate-pulse-study/geant4-issues.md` link to it, and so does the README of that docs folder.
- [ ] `test/dose-rate-patches/README.md` documents prerequisites, commands and runtime.

**Files to Touch:**
- `test/dose-rate-patches/RunAll.ps1`
- `test/dose-rate-patches/README.md`
- `docs/dose-rate-pulse-study/patch-necessity.md`
- `docs/dose-rate-pulse-study/geant4-issues.md`
- `docs/dose-rate-pulse-study/README.md`

**Verification Step:**

Run:
```bash
pwsh -File test/dose-rate-patches/RunAll.ps1 -Reuse
```

Expected:
Three `PASS` lines for sufficiency and three for necessity, `report: .scratch/tests/.../patch-necessity-report.md`, exit 0.

**Notes:**

`-Reuse` uses existing kernels and run results. A full run rebuilds nothing that is already verified. Run `unslop` on the curated doc. Check whether `docs/explorer` lists anything under `test/` or `examples/` and update it if so (the `ExplorerCheck` ctest covers `src/` and `header/` only).
