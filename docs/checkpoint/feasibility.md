# Exact chemistry restart feasibility — ticket 01

Decision: **NO-GO**. Ticket 02 and tickets 03–05 must not start. This is an
unsuccessful exact-restart gate, not an implemented checkpoint feature.

Investigation date: 2026-11-06. Baseline commit:
`87addc009ee2d647d9d442b36539fe45ac3fb57d`, branch
`feat/chemical-checkpoint-resume`. Requested/actual executor: GPT-6 Astra, high
effort, inline. The approved design and tickets remain local in the ignored GPS
session `2026-11-06__during-the-chemical-phase-t-1e-12s-a-simulation-run-should-be`.

## What was established

The isolated executable uses the actual PFLASH detector, physics, PureWater
chemistry, IRT_syn, hand-over and mesoscopic implementation. It compiles
instrumented **copies** of two application sources in its own build directory.
The production application and shared Geant4 source/install were not modified.
No Geant4 patch was applied or tested. No production checkpoint API was added.

Each event receives seed `12345 + 104729 * eventId` before primary generation,
after the run manager's own event-seeding step. The installed engine is
`MixMaxRng`. The fixture contains one run; this seed mapping is an experiment,
not the production multi-run stream design. All reference and cutoff processes
use this same mapping. The uninterrupted reference never installs a checkpoint
time or a cutoff-dependent step. Physics is run once per event in each process;
there is **no resume mode and no physics replay disguised as resume**.

Fixture: 10 keV electrons, PureWater, dissolved O2 at 21%, pH 7, configured
hand-over 5 ns, original particle scheduler end time 20 ns. The actual hand-over
follows the existing PFLASH first-step-at-or-after behavior. Native mesoscopic
initialization chooses its end at particle end minus 1 ps. Both references use
that unchanged behavior. Requested diagnostic cutoffs are 7.123456789 ps
(particle) and 7.123456789 ns (mesoscopic).

All floating-point trace values use 17 significant digits; full engine `put()`
words are recorded without drawing random numbers. Particle traces contain
step clocks, durations, track IDs, species, status, track times and positions.
Mesh traces contain every observed cell/species population, resolution, bulk
counts and RNG state. Reaction callbacks retain ordered event times and stable
track identities. Only the two argument labels within a reactant pair are
canonicalized for comparison. Track states, step order and RNG are not rounded,
sorted away, tolerance-compared or statistically compared.

### Particle cutoff

Every tested event's scheduler clock reaches the requested floating-point value
`0.0071234567890000001 ns`. Its main-list positions and populations are written
to an observation file. This establishes arrival at T under the native
**clipped-step** execution, not exact equality to a state of the unmodified
reference at T.

Inspecting the saved track clocks adds a stricter failure: in event 0 (Serial
and both MT fixtures), live track -2179 has time `0.007123456789000001 ns`
while the scheduler is at `0.0071234567890000001 ns`. Event 2 of the four-event
fixture has the same discrepancy for live track -2188. This one-ULP difference
is not rounded away. `exact=1` in the diagnostic file describes only its
scheduler-clock comparison; it does not certify that every track is at T.
The full per-event audit is in `01-particle-track-times.txt`.

The trace first differs at the crossing step. For Serial event 0, the current
time is `0.0065458139308389946 ns`; the reference selects
`0.00079398546612858591 ns`, whereas the watched-time diagnostic selects
`0.00057764285816100554 ns`. For event 1, the current time is
`0.0070544719900397929 ns`; the respective steps are
`0.0038488881104931094 ns` and `6.8984798960207254e-05 ns`.

Source explains the failure: `G4Scheduler::Stepping` clips at `fStopTime`, sets
interaction-with-medium status and calls `CleanAllReaction()` **before** the
user pre-step callback. Brownian transport then samples a displacement scaled
to that shorter step. The next interaction preparation clears/rebuilds the IRT
cached state. Merely preserving the reaction queue would not establish the
correct intermediate positions or preserve future RNG consumption and process
state. Merely saving the previous state with a T label is also invalid.

The diagnostic calls `Stop()` immediately in the post-step callback at T.
It does not execute subsequent chemistry for that event. Normal termination
still performs cleanup and ordinary in-memory event accounting. Output formats
are disabled and this executable has no production dump/exit flush.

An attempted direct observation of `G4ITReactionSet::Instance()` failed to link
against the Windows shared library (`LNK2001`, `fpInstance`). The final probe
does not manufacture a second singleton. `pendingKnown=0` explicitly means
unavailable; `pending=0` is a placeholder, **not a measured queue size**.
The source's queue-clearing path is independently established. No restorable
pending IRT state or exact physical state relative to the reference is proven.

### Mesoscopic cutoff

The diagnostic sets only the mesoscopic scheduler's endpoint to T after
initialization; the original particle end-time configuration remains 20 ns.
This is a deliberately tested candidate stop mechanism, not an approved
replacement for a pause API. Every event's returned mesoscopic clock exceeds T.
Example final Serial event 0: requested `7.1234567889999996 ns`, returned
`7.1393761719795004 ns` (the initial run returned `7.1275494746461838 ns`).
The complete run-specific values are in
`meso_*/comparison.txt`.

`G4DNAEventScheduler::Stepping` reads the next queued time and returns without
executing that reaction/jump when it exceeds the endpoint. Its caller
nevertheless assigns that later time to `fGlobalTime`, calls `InMesh`, and then
`ResetInMesh()` discards the event queue. Thus the measured clock overshoot does
**not** establish that the future reaction executed. It does establish that
normal return is not an exact-time, queue-preserving pause. The observed mesh
and bulk populations alone cannot reconstruct sampled future work. After this
diagnostic return the particle scheduler is stopped; no hidden continuation to
the original final time is performed.

### Uninterrupted oracle

Repeat fresh processes were run for Serial/2 events, MT/2 workers/2 events, and
MT/2 workers/4 events. The initial comparator reported raw A/B reactant-label
swaps as mismatches. Those labels were canonicalized; all numerical data and
ordered reaction identities remain exact. Recomparison of the initial Serial
and two-event MT traces then matched (4,782,309 and 4,739,365 lines per event).
Those initial raw-label failures are not evidence of scientific divergence.

The initial four-event MT comparison still failed after canonicalization:
event 0 selected steps `0.036818740755803478 ns` and
`0.036818522877316906 ns` from the same clock `3.1936901617353204 ns`.
Event 3 also differed. This is a numerical execution difference, not a renamed
reactant pair. Its root cause has not been isolated; event seeding alone has
not established a repeatable oracle when events outnumber workers. No causal
claim about a particular race or pointer ordering is made from this observation.

The final rerun also exposed Serial instability: event 0 differed at the same
3.1936901617353204 ns clock, and event 1's engine words differed at trace line
9,599. Thus an earlier matching pair of Serial executions does not qualify
repeatability. A pass of an oracle test means only that its observed
uninterrupted traces matched in that pair, not that restart or complete
internal-state equality has been demonstrated.

## State inventory and restoration gaps

This is the identified minimum inventory, not a certified exhaustive serializer
schema. An unclassified dependency is a NO-GO, not permission to omit it.
"Hook needed" means an explicit, versioned API in an isolated Geant4 build;
raw pointers, object memory images and a private/public preprocessor trick are
not a restoration strategy.

| Owner | Mutable state that matters | Capture/restore route and status |
|---|---|---|
| `G4Scheduler` | Global/start/stop/end/tmp clocks; selected/previous/TS/IL time steps; user time limits, watched times and step map; step count and zero-time count; interaction/step status, running/continue/initialized flags; model and processor phase | Some scalar getters exist. Private state and a resume entry point bypassing `Process()->Reset()` need hooks. Must pause before clipping/cleanup, and preserve the original endpoint. Unimplemented. |
| IRT time-step computer | `fIsInitialized`, `fSampledPositions`, `fCheckedTracks`, `fSecondaries`, `fRCutOff`, sampled minimum step; base current time/user minimum time/reactant vector; reaction model cache and process links | Stable track/configuration IDs plus exact numeric values; explicit export/import of private fields. No supplied serializer. Reinitialization samples again and is not restoration. |
| IRT reaction set / maker | Pending times, ordered reactants, equal-time insertion order, per-track and per-time membership; `G4DNAMakeReaction::fTimeStep`, selected reaction state | Rebuild cross-indexed lists from identities in original traversal/insertion order. Windows singleton access failed at link. `GetHash()` hashes the first reactant ID twice; same-time collisions and ordered orientation require audit, not blind sorting. |
| Track holder | Main/secondary/waiting/delayed/kill lists, list/species ordering, next negative track-ID counter, post-activity global time, membership/leader flags | Public list access helps observation. Restore list membership, original IDs and allocation counter together; normal `Clear()` resets IDs to -1. Hooks and ownership discipline required. |
| Tracks and molecules | Configuration/electronic state, IDs, status, positions, global/local/proper times, momentum/energy, steps/pre/post points, user data, touchables | Public construction/setters cover part of it. Species must resolve through saved stable keys. Creating only `G4Molecule::BuildTrack(T, position)` loses process/history state. Not round-tripped. |
| Tracking/step/process state | `G4TrackingInformation` recorded position/time, leader, navigator and step-processor state; per-process state vector; selected DoIt vectors, previous step and safety; Brownian endpoint/time-limit/random/cache values; interaction lengths left | The lock/state-pointer APIs are in-process state switching, not portable serializers. Typed accessors and a restore path that does not rerun `StartTracking` resets are needed. The arbitrary-T Brownian bridge/continuation semantics remain unresolved. |
| Particle scavenger process | Previous pre-step time, material-valid flag, residual interaction lengths/current interaction length; selected material/configuration and returned value; reaction/material traversal | Preserve typed state per process/per track and exact stable traversal. `PostStepGetPhysicalInteractionLength` consumes randomness and rebuilds cumulative propensities; rerunning it is not a neutral restore. |
| Scavenger material | Exact int64 species pool, time-counter histories/comparer, search-cache cursor, initialized/counter flags, equilibrium state | Pool iterators exist; do not serialize the double-returning observer as a general exact int64 format. Histories/cursors/equilibrium require hooks. Fixed water/pH special rules must remain. Probe observes API-visible bulk counts, not complete pool internals. |
| Equilibrium controllers | Status/add-equilibrium flags, equilibrium/global time, duration, reaction type and reactant/bulk identities, in both scavenger and Gillespie owners | Explicit scalar/key export/import. Recalling `Initialize`/`Reset` loses history. Not captured by population plus RNG snapshots. |
| Mesoscopic scheduler | Start/global/end/selected time, transfer time, pixel level, running/initialized/change flags, step/reaction/jump counters, record times/cursor/counter map | Exact pause-before-next-event API and restore entry point needed; must avoid `ResetInMesh`, `InitializeInMesh` and `Reset` during save. No existing complete capture/restore API. |
| Mesh | Bounding box, resolution, ordered voxel vector and populations, index lookup, neighboring-cell traversal, current coarsening level | Public iteration and `InitializeVoxel` permit a proposed reconstruction in original vector order, rebuilding the index lookup. Preserve counts and cell identities; repeated revoxelization is not restoration. Not round-tripped. |
| Mesoscopic event set | Each sampled time, cell, reaction identity or species/destination jump, one-event-per-cell lookup and ordered queue | Time/index ordering is explicit in `G4DNAEventSet`. Store by stable keys and reconstruct lookup/iterators. Scheduler's owning queue is private and cleared at return. Not captured. |
| Gillespie/update models | Current time, cumulative reaction/jump propensities, equilibrium histories, mesh/event-set/scavenger aliases, selected update phase | Capture durable state; rebuild only demonstrably derived caches without random draws, in the original order. `CreateEvents()` draws new r1/r2 and cannot restore the queue. No proof of equivalent rebuilding. |
| RNG and event dispatch | Engine type, complete engine words, distribution caches if used, seed mapping, event/run IDs, assignment state | `put/get` supports engine state. `G4RandGauss` aliases `RandGaussQ`; do not assume every distribution is cache-free. Probe records engine state, but no restore was exercised. MT excess-event oracle still unresolved. |
| Molecule/reaction counters | Counter activation/muting, per-species histories, fixed time comparers, cached queries, per-run/event reset policy | Need explicit histories plus cursor/flag export. Existing 10 ps scoring precision does not define or permit rounding checkpoint T. Rebuilding counters by replaying chemistry is forbidden. |
| PFLASH stage action | Hand-over flag, meso enabled flag, counter-active/muted flags, record grid/next index, partial species records, spatial data/species columns, partial reaction counters | Add application-owned snapshots and pause lifecycle. Wall timers are excluded from scientific equality. `EndProcessing` currently finalizes/resets, so it cannot be reused as checkpoint completion. |
| PFLASH event/scoring/run | Energy deposit/hits, current and committed sums/squares/G-values, interaction counts, track-length rows, prechemical rows; aborted/completed state; aggregate merge order | Persist partial event separately from committed results. Deterministic event-order aggregation is required for MT; current merge/addition order is not qualified. Public totals alone omit `fCurrent` fields. |
| Output/lifecycle | Prior accumulated runs, pending data, used prefixes/subdirectories, staged files/root rows, manifest/configuration and original final time | Separate checkpoint publication from normal event/run/dump paths. Must preserve prior results and commit each resumed event once; suppress original macro suffix. None of this was implemented in ticket 01. |

Ordering concerns are concrete audit targets, not proven causes of the MT
mismatch: IRT equal-time hash/insertion behavior; pointer-keyed scavenger
material traversal; voxel-vector order; molecular-configuration iteration;
floating-point propensity accumulation; and scientific run reduction. Stable
serialization identifiers alone do not fix these execution-order dependencies.

## Source evidence and example reuse

Paths below are relative to the **resolved `geant4SourcePath`** from project
configuration. Version 11.4.3 implementation files, not just headers, were read.

- `source/processes/electromagnetic/dna/management/src/G4Scheduler.cc`:
  `Reset` at 254; `Process` at 273 (reset, scavenger reset, end action, track
  deletion/reset); `SynchronizeTracks` at 382; clipping at 629–636, transport
  and reaction dispatch at 675–701.
- `.../dna/models/src/G4DNAIndependentReactionTimeStepper.cc`: `Prepare` at 60;
  saved pending reactions/positions at 256–260; reaction selection/restored
  sampling origins at 263–327; initialization and future-time selection at
  360–438. `G4DNAMakeReaction.cc` samples reaction positions and products.
- `.../dna/processes/src/G4DNABrownianTransportation.cc`: `ComputeStep` at 184,
  Gaussian displacement at 235–240 and boundary transport at 381–387;
  `AlongStepDoIt` at 799. `G4DNAScavengerProcess.cc` samples material choice and
  maintains residual interaction state in its interaction-length method.
- `.../dna/models/src/G4DNAEventScheduler.cc`: `Reset` at 175;
  initialization at 207; `ResetInMesh` at 267; loop/clock assignment at
  375–383; unconditional loop cleanup at 454; reject-future-event at 483.
  `G4DNAGillespieDirectMethod.cc` draws new randomness in `CreateEvent` and
  owns equilibrium controllers. `.../dna/utils/src/G4DNAEventSet.cc` orders by
  time then cell index, and maintains the companion cell lookup.
- `.../dna/utils/src/G4DNAScavengerMaterial.cc`, `G4ChemEquilibrium.cc`,
  `G4DNAMesh.cc`; `.../dna/management/src/G4ITTrackHolder.cc`,
  `G4ITReaction.cc`, `G4TrackingInformation.cc`, `G4ITStepProcessor.cc`;
  `source/run/src/G4RunManager.cc` and `G4WorkerRunManager.cc` were inspected
  for pools, histories, identity/order, process-state switching and seeding.

The installed-version example action implementations were inspected before
writing the probe:

| Example | Reused pattern / missing capability |
|---|---|
| chem1 | Primary generator plus stacking action launches chemistry; no restart mechanism. |
| chem2 | Thread-local scheduler time-step action and step-limit demonstration; stopping is not persistence. |
| chem3 | Chemistry tracking interactivity and time-step callbacks support observation; prechemical output is initial-state output, not mid-chemistry restart. |
| chem4, chem5, chem6 | Per-event/run counter reset and solvent exclusion; preserve those policies rather than treating a paused event as completed. |
| molcounters/basic and sdcounters | Counter registration/activation and chemistry tracking hooks; counter histories and callbacks add state, not a complete scheduler snapshot. |
| UHDR | Scavenger installation, particle-to-mesh hand-over, per-thread mesoscopic scheduler, mesh initialization and normal reset. This is PFLASH's established model; its examples do not export/import pending work. No UHDR pulse behavior was added. |

## Integration needed before another GO attempt

There is no tested minimal patch to recommend. A bounded follow-up would first
need to establish a cutoff-independent, repeatable MT oracle, then provide:

1. A particle suspend/resume phase that retains scheduler/process/IRT state
   without reset, resampling, clearing or completing future work.
2. A demonstrated definition and construction of particle state at arbitrary T
   that preserves subsequent positions and RNG against the unmodified
   uninterrupted mode. A queue getter or a Brownian split alone is insufficient.
3. A mesoscopic pause before dequeue/clock overshoot and before mesh cleanup,
   plus typed export/import of queue, mesh, equilibrium, clocks and record state.
4. Stable traversal/identity guarantees and application partial-scoring state.

Any implementation must be maintained as a version-controlled patch against
11.4.3, retaining Geant4 license notices, and compiled in a separate source/
install tree. No shared source/install edits are authorized by this report.
No scope fallback (SBS, statistical equivalence, rounded T, physics replay,
future computation during save, or a cutoff-dependent reference) was used.

## Reproduction and artifacts

Use the project MSVC x64 setup from `.claude/geant4-instructions.md`. Resolve
configuration variables as required by `~/.claude/CONFIG_RESOLUTION.md`; do not
replace them with machine paths in this fixture. From the project root, in a
PowerShell with the MSVC x64 environment loaded:

```powershell
$config = Get-Content .claude/.claude-project.json -Raw | ConvertFrom-Json
if (!$env:DEV_DIR -or !$env:G4_ROOT) { throw 'Run setup-dev-env.ps1 and restart the shell' }
$g4Install = $config.geant4InstallPath.Replace('${DEV_DIR}', $env:DEV_DIR).Replace('${G4_ROOT}', $env:G4_ROOT)
$toolchain = $config.build.toolchainFile.Replace('${DEV_DIR}', $env:DEV_DIR).Replace('${G4_ROOT}', $env:G4_ROOT)
$evidence = Join-Path (Get-Location) '.scratch/tests/2026-11-06__during-the-chemical-phase-t-1e-12s-a-simulation-run-should-be/01-artifacts'
cmake -S test/checkpoint/probe -B build-checkpoint-probe -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DGeant4_DIR=$g4Install/lib/cmake/Geant4" `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DPROBE_ARTIFACT_DIR=$evidence"
cmake --build build-checkpoint-probe --parallel 6
$env:PATH = "$g4Install/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
ctest --test-dir build-checkpoint-probe -L checkpoint-feasibility --output-on-failure --no-tests=error
```

The live-kernel executable uses RelWithDebInfo to match the installed runtime;
this is a smoke/probe executable, not a plain-assert unit-test suite. All
validation uses explicit runtime checks and nonzero exits, so `NDEBUG` cannot
turn a failure into success. No production-size run or GUI was launched.
Traces are large (hundreds of MB per event) because exact observations are
retained, not summarized by a potentially colliding digest.

Compiler: MSVC 19.51.36256.0, x64, C++20; Geant4 reports
`geant4-11-04-patch-03 [MT]`, version number 1143, 01-October-2026. Optional
application HDF5 output is off; linked Geant4 dependencies remain as installed.
The baseline mesh emits the documented `WrongResolution` warning for O^-1 +
H3O^1 (6.3164 nm radius, 15.258789 nm cells). This is recorded, not treated as
the cause of the exactness failure. Build initialization also emits an existing
`vswhere.exe` lookup warning inside the MSVC environment batch; compiler
detection/build completes despite that warning.

SHA256 of the probe executable:
`BC73C1D5074161BFE3E5FA6CCF72D84F4FA9913B46AA99219DBB1D916914B611`.
SHA256 of installed `G4processes_core.dll`:
`4EA4EFACF90F9D69D3C1AB2943A0DE4CD54E367F9B0F6688C8B6161C57E787D7`.
Source and final comparator fingerprints are retained alongside the logs.

Artifact root:
`.scratch/tests/2026-11-06__during-the-chemical-phase-t-1e-12s-a-simulation-run-should-be/`.
`01-configure-fresh.log`, `01-build-final.log`, `01-comparator-build.log`,
`01-ctest-final.log`, `01-boundary-prefix.txt`, `01-particle-track-times.txt`, `01-fingerprints.txt`,
`01-library-fingerprint.txt`, `01-comparator-fingerprint.txt` and
`01-artifacts/*/comparison.txt` contain the
commands' evidence. Per-process `.log`/`.err`, per-event `.trace`, `.capture`
and `.mesh` files are retained under each case. `.capture` explicitly records
`restorable 0`; it is not a checkpoint schema.

The initial missing-ZLIB configure was corrected by a fresh probe CMake cache
using the configured vcpkg toolchain. Build bring-up also exposed the singleton
link limitation. These are recorded separately from scientific failures.

### Final verification result

Command: `ctest --test-dir build-checkpoint-probe -L checkpoint-feasibility --output-on-failure --no-tests=error`.
Exit **8**, **1 passed / 8 failed / 0 skipped / 9 selected**, 260.79 seconds.

| Case | Final result | Evidence |
|---|---|---|
| `oracle_serial2` | FAIL | Event 0 step duration and event 1 RNG state differ across fresh processes. |
| `particle_serial2` | FAIL | Both scheduler clocks reach T; a live track clock differs, pending state unavailable, no restorable capture. |
| `meso_serial2` | FAIL | Both returned clocks exceed T; pending queue not retained/exported. |
| `oracle_mt2` | PASS | Both complete observed traces match after reactant-pair canonicalization. |
| `particle_mt2` | FAIL | Both scheduler clocks reach T; a live track clock differs, no restorable capture. |
| `meso_mt2` | FAIL | Both returned clocks exceed T; no restorable capture. |
| `oracle_mt4` | FAIL | Events 0 and 1 select different step durations; events 2 and 3 match. |
| `particle_mt4` | FAIL | All four scheduler clocks reach T; two live track clocks differ, no restorable capture. |
| `meso_mt4` | FAIL | All four returned clocks exceed T; no restorable capture. |

All subprocesses in this final run returned 0 and emitted their completion
marker; none of the eight failed cases is a missing executable, timeout or
skipped test. The failed capture tests explicitly reject the absent queue and
restore capability. They are **not** failed comparisons of an implemented
serializer. The final four-event MT first mismatches are at lines 701,812
(event 0) and 620,843 (event 1); the latter compares steps
`0.026406009466242924 ns` and `0.02640496802469517 ns` at the same clock
`4.1107965776971485 ns`.

## Limits and ticket disposition

No fresh-process **restore** was performed, because neither cutoff mechanism
produced a complete restorable event. No equality of positions at T with the
uninterrupted physical trajectory, no full internal-state round trip, no
post-restore RNG continuation, no deterministic scientific aggregation, no
all-events transactional checkpoint, no repeated checkpoint, and no production
output equivalence are claimed. Mesoscopic reaction/jump identity is inferred
only through observed mesh changes; the private event payload is not traced.
These are failed/unmet gate requirements, not passing or skipped evidence.

The gate tests remain red when any required capture capability is absent;
they are not marked `WILL_FAIL`. The probe is intentionally incapable of
certifying GO from observation files. GPS ticket 01 must remain blocked rather
than Done. The investigation is preserved for review; no commit, push, PR,
finish command, or implementation of tickets 02–05 is performed. The user's
existing `CONTEXT.md` addition and ignored approved session are preserved.
