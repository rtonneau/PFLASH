# Private molecular finder correction (Geant4 11.4.3)

This investigation starts at PFLASH `c61329172a705dd05c34c41eb63df0a54cb068e3`. The correction is an experimental patch to a private pristine Geant4 11.4.3 source/build/install. It does not include the earlier timed-injection or mesoscopic patches and does not establish full-dose feasibility.

## Established cause

`G4Octree.icc` creates a fixed leaf for at most two entries. For three or more it partitions the bounding box, independently copies every entry contained by each child box, and recursively constructs each nonempty child. `G4DNABoundingBox.cc::contains` includes both endpoints. A point on one, two or three split planes therefore has multiple owners. For an entirely coincident cloud all eight boxes equal the parent and each receives all entries.

This is **finite-depth combinatorial duplication**, not infinite recursion. The maximum depth is 100, but the original `++current_depth` also increments the parent's local depth across siblings. The recurrence for the coincident case is `T(d) = 1 + sum(T(d+i), i=1..8)`, with a vector leaf when `d>100`. The resulting enormous finite tree exhausts practical memory. Every child node owns its own allocation; the duplicated objects are iterator/position entries, not distinct molecular tracks.

Evidence is complementary:

- The preserved prior finder isolation has two coincident molecules finishing, three/four reaching the 256 MiB committed-private-memory cap, and distinct-position controls passing. Its raw files remain unchanged.
- A fresh baseline constructor trace uses copied pristine headers with only an entry counter/log and a 1000-node diagnostic stop. Every logged node retains three entries and the identical box; leaf depths 103 and 107 occur while sibling construction continues. It is a diagnostic stop, not a successful finder build.
- A native bounding-box probe confirms 24 child entries from three input entries and eight identical boxes over six repeated partitions. It does not allocate the enormous tree.
- Fresh uninstrumented baseline three/four-point oracle processes also hit the hard cap. The two-point oracle exposes a separate zero-radius expanding-query stack overflow; the original two-point construction itself succeeds.

## Correction and query contract

[The patch](../../patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch) changes `G4Octree.icc`, `G4OctreeFinder.icc` and one contact guard in `G4DNAMakeReaction.cc`:

1. Assign each entry to exactly one child using midpoint comparisons; equality belongs to the upper half. Keep inclusive child boxes for geometric bounds.
2. Store an all-coincident population in the existing vector leaf. Stop if all entries enter a child whose representable bounds equal the parent. Otherwise permit a shrinking one-child subdivision, and pass `current_depth+1` independently to siblings. Retain the existing maximum-depth fallback.
3. Include distance-zero entries in position-based tree results. Track-based wrappers remove only the exact querying `G4Track*`, before the expanding-radius neighbor-count decision. Equal IDs are not identity. Position-only queries have no self identity and retain all matching tracks.
4. Preserve strict `distance < radius`, sorting and positive-radius expansion. Zero or negative radius no longer recursively multiplies itself in the expanding overloads; a zero-radius sphere is empty under the strict comparison.
5. Prune using the nearest point clamped to the stored child bounds. The original center/half-width arithmetic loses endpoint inclusion through rounding for a near-ULP cloud: querying `(1,1,1)` at radius `1e-17` lost all 14 coincident peers. Direct-bound pruning fixes the independently observed oracle failure without changing the global bounding-box class.

6. Permit distinct coincident reactants to retain their positions when `G4DNAMakeReaction` has a zero stored position-update interval. The first finder-only candidate passed all 14 oracle cases but native contact aborted with `G4DNAMakeReaction003`; its DLL, patch and failure logs are preserved. The narrow return avoids normalizing a zero vector, changes no nonzero-distance path, and retains the original rejection for a positive stored interval and a mobile self pair. Direct public-API regression fixtures observe both retained guards through an exception handler that throws immediately, rather than continuing after a fatal condition.

The IRT stepper's `FindReaction` rejects the same track pointer, restores sampled positions, then calls `MakeReaction` directly. It does not call `TestReactibility`; the reaction process's position-update interval remains its constructor value zero on that path. That field is therefore not evidence of the physical elapsed reaction time. Native contact tests independently require electron-electron reaction time to equal the 1 ps start.

The native `G4DNAIndependentReactionTimeStepper.cc::CalculateStep` already rejects the same track pointer and supports a contact reaction inside the effective radius at scheduler start. `G4DNAElectronHoleRecombination.cc::FindReactant` explicitly assigns probability one at zero distance. Previously the finder suppressed those distinct coincident partners before either branch could use them.

## Validation

All **55 final capped cases passed**: 14 brute-force/structure cases, 39 native contact cases and two retained-guard cases. All 24 ordinary native controls (1 to 4 electrons, 0.01/100 nm spacing, three seeds) had exactly the same reaction/electron counters as the unmodified baseline. The ordinary random external-query oracle passed both kernels. Private DLL inventories and before/after runtime hashes passed for every final case.

The largest observed job-reported process private-memory peak was **12,050,432 bytes (11.5 MiB)**; maximum sampled working set was 26,124,288 bytes. The longest complete test process was the 4096-coincident-point oracle: **2.76335 s**, with 5,464,064 bytes peak private memory. This includes startup, repeated queries, sorting and brute-force assertions; it is not a finder construction-time benchmark or a scaling estimate. Minimum system free memory across the suite was 10,893,090,816 bytes. Every case stayed below its 256 MiB/5 s limits without a cap notification.

The installed private `G4processes_core.dll` SHA-256 was `353A78F7A53BBBB57C2C482B98BE3F07A963B56B00DEDAC6BE072291D446CF75`. All 8392 source-tree files were compared with the configured source; only the three patch-owned files differ. Shared source targets, shared DLLs and the preserved original raw finder evidence matched this session's protected before/after hashes. The patch applies cleanly to pristine source and reproduces the private source bytes with LF preserved. The complete private build/install finished in 1562.09 s under its one-hour/4 GiB-per-process cap; the final incremental contact correction took 20.50 s including probe rebuild.

The oracle compares exact pointer sets, rejects duplicate results, verifies returned distances and sorted order, and uses the same strict floating-point Euclidean distance as the API. It covers coincident populations through 4096, split-plane/edge/corner lattice points, a line, near-ULP coordinates, adjacent representable coordinates, deterministic random clouds at both ordinary and molecular scales, external ordinary queries, multiple species keys, identity despite duplicate track IDs, and keyed positive-radius expansion against an independently enumerated final radius. It tests all four public query overloads. An absent key is tested with initially empty output; this patch does not redefine the pre-existing behavior of an already populated output for absent keys.

Native fixtures run the installed private kernel with the application's pure-water chemistry, IRT_syn and no beam. One to four electrons start at 1 ps, at identical positions, 0.01 nm spacing or 100 nm spacing, across seeds 1729/1733/1741; the end is 1.001 ps. They verify immediate electron-electron contacts, absence of self reaction, electron reaction balance and separated controls. For three/four electrons, products can undergo further reactions, so the assertion is at least one initial electron-electron contact plus exact electron balance, not an unjustified fixed final yield. An ionized-water/electron coincidence also tests the native zero-distance recombination path; the electron must be consumed. These are controlled contact tests, not beam-yield validation.

Every final runtime case starts suspended inside a Windows Job Object with a 256 MiB process committed-private-memory cap and kill-on-close, then runs with a five-second deadline and 3 GiB system-free-memory floor. Logs contain exit status, cap notifications, elapsed time, sampled working set, job-reported peak private memory and loaded module paths. A working-set measurement is not the committed-memory cap. Tests require the success marker, exit zero, no cap event, and every observed Geant4 DLL in the private install, including the core DLL. Executable/runtime hashes are checked before and after the suite.

## Reproduction

Read `.claude/.claude-project.json`, expand `DEV_DIR`/`G4_ROOT` and follow `~/.claude/CONFIG_RESOLUTION.md`. Use the configured pristine source; never apply this patch to the shared source/install. Copy it to a fresh private directory (exclude `.git`), then `git -c core.autocrlf=false apply <absolute-path-to-patch>` inside the copy. Keep separate source, build, install, probe and evidence directories. This patch applies directly to pristine 11.4.3; do not apply the earlier mesoscopic patches for this isolation.

Configure/build/install with the project's MSVC x64 environment and matching Release/Ninja options: multithreading on, system zlib and HDF5 on, Qt/OpenGL off, no dataset downloads, vcpkg toolchain. On this machine FindHDF5 additionally required explicit `HDF5_ROOT`, `HDF5_C_INCLUDE_DIR` and `HDF5_hdf5_LIBRARY_RELEASE` pointing inside the resolved vcpkg installation. Re-run the build after any header edit before installing. The source patch is compiled into the native core DLL; copying installed headers alone is insufficient.

Configure `test/molecular-finder` with `-DGeant4_DIR=<private-install>/lib/cmake/Geant4`, the vcpkg toolchain, and Release. Assertions are explicit exceptions, never disabled `assert()`. Build the targets, then:

```powershell
./test/molecular-finder/Validate.ps1 -BuildDir <probe-build> -InstallPrefix <private-install> -OutDir <new-evidence-directory>
```

For a fresh baseline configure a separate probe against the shared **unmodified** install with `-DFINDER_BASELINE_SOURCE=<configured-source>`. `FinderTrace coincident 3` uses diagnostic-only copied headers; `OwnershipTrace` uses native bounding-box operations. Run all baseline executables under `JobLauncher 256 5 <new-prefix> <executable> [arguments...]`. Baseline failures remain failures; they are not converted into passing expected-failure tests. The launcher returns its own status; always inspect the child exit/reason in `.meta`.

For bounded builds, first compile `JobLauncher.cc` with MSVC and `psapi.lib`, then launch a PowerShell script containing the MSVC configure/build/install commands through `JobLauncher 4096 3600 <prefix> powershell.exe -NoProfile -File <script>`. The measured full build used `-j4`; each process is capped at 4 GiB, with the same system-free floor. Use fresh log prefixes. A build cap or timeout is a failure, not a reason to remove the cap.

## Evidence and limits

Local raw evidence: `.scratch/tests/2026-11-08__molecular-finder-degenerate-coordinates`. Fresh baseline executables, diagnostic headers and test source are frozen in `baseline-frozen`; the finder-only candidate failure is preserved in `finder-only-failure` and `validation`. The previous raw baseline remains under `.scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/08-meso/finder-coincident`. The earlier raw hash manifest points to live paths that had already changed before this session: its finder executable was rebuilt at 08:47:59 and its core was replaced at 04:09:07 on November 8. The recorded F864 core is preserved separately in the prior `frozen-core-f864` directory. This investigation leaves those paths and raw evidence untouched and does not claim the current old executable reproduces its recorded hash; `prior-baseline-drift.csv` records the discrepancy. Fresh pristine-baseline failures provide reproducible independent evidence.

Scratch is ignored: archive both separately for independent replication. Tracked fixtures and the patch are sufficient to reproduce; absolute machine paths in raw records are provenance, not source configuration.

This validates finite, ordinary molecular coordinates and these bounded fixtures. It does not establish behavior for NaN/infinite coordinates or overflow-scale distances, global stochastic equivalence, full-packet performance, recurring-source accuracy, or production readiness. A coincident vector leaf necessarily scans its population and can return O(N) neighbors; the correction removes pathological duplication, not the real cost of a dense reaction neighborhood. Other pre-existing octree API/lifetime issues are outside this correction. The previous mesoscopic **NO-GO** remains in force. No shared Geant4 files, full-dose simulation, checkpoint implementation, PR or push are part of this ticket.

