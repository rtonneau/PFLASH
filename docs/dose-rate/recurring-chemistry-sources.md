# Requirements and a validation prototype for recurring chemistry sources

Local implementation and runtime validation, updated 2026-11-08. The bounded private-patch particle reference is ready for ticket 3 experiments; full beam feasibility is not established.

## Current continuation status

The unchanged private Geant4 build now passes the installed-library pilot
equivalence checks (8 independent packets, 64 chemistry seeds, 1024 histories).
DLL-path inventories and source hashes are retained. A fixed molecular fixture
demonstrates the rejected adapter's executable/DLL finder-state split, with
matching reaction tables and pre-sampling engine states. A fresh direct-generation
comparison also passes all measured species at 64 chemistry seeds per packet;
every regenerated precursor packet matches the preserved original byte for byte.

The private DLL source-contact candidate v2 passes 33 registered controls,
including controlled bulk depletion, callback accumulation and near-end arrival,
and records same-time descendant cross-reactions in a separate 32-seed fixture.
Its 128-seed chemistry comparison passes for every measured species under the
unchanged exploratory margins (eight physical clusters, 2048 histories).
Real Poisson schedules exposed premature counter registration during future-track
construction; corrected activation-time registration passes empty, single and
multiple-packet histories, including actual molecule-counter/live-population checks.

Complete synthetic histories pass in native Serial and two-worker MT, across
repeated runs with reused source IDs. Four actual packet schedules also pass in
Serial and two-worker MT, with identical source schedules and budgets.
Yield/time/dose and measured cost artifacts are available. The current candidate
acceptance suite passes 36/36 checks, including simultaneous combined-cloud and
distinct delayed controls with immediate counter and dose-ledger checks. A
32-history reuse study demonstrates substantial bias from exact spatial reuse;
it does not validate an unbiased independent beam. The additional 2048-history
distant-source boundary and 384-history combined-cloud ensembles now pass their
predefined exploratory chemical comparisons. Final evidence audits pass and
independent review has no unresolved actionable findings. The particle reference
is ready for bounded ticket 3 comparisons; ticket 3 itself is not validated.
The stronger proposed pending-clock
contract is a separate design proposal, not the ticket's acceptance criterion.
See the final continuation section for current evidence. Entries below preserve the chronology:
earlier statements that private builds/tracing had not been performed are
historical, superseded by the continuation evidence near the end.

## Evidence categories

Demonstrated limitation (prior retained experiments): installed Geant4 11.4.3 IRT_syn rejects the delayed synthetic contacting electron pair while simultaneous and shifted startup controls pass. Its stepper contact branch requires globalTime == startTime. This is a source-event limitation, not evidence that recurring irradiation is physically infeasible.

Adapter issue: the renamed three-class executable copy fails the existing packet-cluster statistical baseline comparison even with correction disabled. Eight physical packets with two chemistry realizations each gave hydrated-electron difference 928.31, CI95 [655.30,1201.33], versus pilot margin 200.81; H2O2 difference -15.31, CI95 [-19.14,-11.49], versus margin 1.46. These are previous results, not new runs. The installed kernel with the same history driver agreed closely with the original direct-generation reference.

Hypothesis, not established cause: executable and DLL template singleton state may differ. Configured source G4DNAIndependentReactionTimeModel.cc initializes the chemical molecule finder and enables octree use. G4DNAElectronHoleRecombination.cc samples a random probability before selecting octree or KD-tree lookup from IsOctreeUsed; candidates and the first candidate's probability determine its return. G4OctreeFinder.icc defines a thread-local template instance. These source facts do not measure runtime instance identity.

New control preparation: 03-pristine/probe is an isolated copy of the probe with the original contact condition and all SourceContacts references removed from kernel and drivers. Windows class renaming, reaction-set accessor substitution and minimum-step ABI substitutions remain. Therefore this is a contact/bookkeeping-free adapter control, not an unchanged Geant4 kernel build. Earlier control-p0 runs performed before the corrected driver rebuilt had no populated chemistry and are INVALID; do not use their PASS marker as acceptance. Final corrected evidence must contain species counts.

## Proposed kernel contract

An explicit source activation operation should accept tracks and their physical activation timestamps as a batch. Validate configuration compatibility and unique identities before insertion; do not evolve them before activation. Synchronize the old bath to that time, activate the complete same-time batch, and refresh neighbour search before sampling contacts. Specify ordering with coincident reactions and make empty activations inert.

Search both new-new and new-old pairs, including same-time descendants where required by the chemistry model. Do not replay all old-old contact trials. Invalidate or update reaction schedules affected by incoming tracks, consumed reactants and new products. Preserve unaffected old chemistry clocks, bulk/scavenger populations, environment, scoring and random-engine ownership. Define the interaction with dissociation and electron-hole recombination explicitly; merely broadening one stepper condition is not a validated implementation of this contract.

Keep interacting chemistry components within the same Geant4 library in an isolated private source/build/install. First establish unchanged-library baseline parity; only then implement the smallest source-event change. Do not add further renamed-template or TLS-access shims to repair an unproven diagnosis. Shared source and install must remain untouched.

## Required validation

Trace a fixed molecular fixture at model initialization and first electron-hole decision on both executable and DLL sides: instance address, octree-used/built flags, initial molecular configurations and populations, reaction table identity/content, candidate IDs and distances, full engine state before sampling, sampled probability, selected reaction and products. Locate the first meaningful divergence. Seeds are diagnostic aids; final acceptance is statistical, not trajectory equality.

Regression controls must include no source, empty source, simultaneous union, shifted startup, delayed contacts, new-old and new-new contacts, nonreacting population accounting, multiple activations, same-time descendants and near-end activations. Check exact packet/energy budgets and inactivity before activation. Test bulk/scavenger continuity, scoring continuity, and independent complete histories in Serial and small MT runs.

Predefine species/time observables and scientific equivalence margins. Compare paired differences clustered by independent physical packet; chemistry replicates within a packet are not new physical samples. The existing 5%-or-one-molecule margins are exploratory, not production accuracy approval. Report bias separately from inconclusive rare-species comparisons; increase sampling for inconclusive results without moving margins. Existing between-packet variation includes history noise and is not a pure variance decomposition. Reused direct-reference rows are not independent realizations. No familywise coverage is established by marginal intervals.

Ticket 2 requires all acceptance controls and credible particle chemistry parity. Ticket 3 remains gated. No validated source patch, private-library parity or runtime singleton diagnosis is claimed in this report.

## Provenance

Workspace branch wip/precursor-capture-probe at 84fca374a426788319d514437c443b22979d001a was verified. DEV_DIR and G4_ROOT resolved from the environment; configured source is DEV_DIR/GEANT4/geant4-v11.4.3-source. Prior evidence in 01-msvc and 02-msvc is preserved. New isolated artifacts are under .scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/03-pristine. No production source changes, commit, PR, publication or upstream messages.

## New populated runtime control

After the corrected build finished, final-control-p0.txt/log records packet library0, seed 81001, one startup packet, endpoint 2 ps: e_aq 6252 and H2O2 14. The preserved installed p0r0m0 result is e_aq 5057 and H2O2 42; the previous copied p0r0m1 result is 6240 and 22. This single diagnostic shows the discrepancy persists with source-contact hooks removed; it is not a new statistical equivalence test. A recursive search found no SourceContacts references in the generated kernel or either control driver. The control retains Windows ABI adaptations and cannot establish pristine library parity.

Outstanding requested work: executable/DLL runtime tracing, fixed electron-hole fixture, unchanged private Geant4 build and minimal patch if indicated, statistical validation and ticket 2 acceptance. These have not been completed. The technical route is not exhausted.

## Private unchanged build, 2026-11-07 (validation in progress)

Created `04-private/source`, `build`, and `install` under the session scratch root.
Geant4 build/install and the unrenamed PacketHistoryInstalled driver build returned
zero. `shared-cache.txt`, `options.cmake`, `build-command.txt`, `build.log`, and
`probe-build.log` preserve configuration and build evidence. Release/C++17,
multithreading, HDF5 and system zlib match the configured shared build. Existing
data and vcpkg dependencies are reused; data installation is disabled.

`RunPrivateParity.ps1` runs 8 independent packets x 2 chemistry replicates against
both installations and captures actual G4 DLL module paths in every process.
The first private result (packet 0, seed 81001, 2 ps) contains hydrated electron
5057 and H2O2 42, matching the installed diagnostic. Private module paths resolve
to `04-private/install/bin`. This single pair is not statistical acceptance.
The ensemble is still running; no source activation patch or runtime finder trace
has been validated. The unchanged build is retained separately from any future
instrumented build. No shared source/install modifications or completion commit.

Completed private baseline pilot: 32 histories (8 packets x 2 chemistry seeds x
2 libraries), all with captured G4 module paths in the expected installation.
Source hash comparison verified all 8392 source files. Private DLL hashes saved.
`04-private/parity/comparison.txt`: e_aq delta -1.375, CI95 [-5.0453,2.2953];
H2O2 delta -0.9375, CI95 [-3.3176,1.4426], margin 1.53125. H2O2 and HO2
are inconclusive; other species and total population meet the pilot margins.
No species is classified different. Thus the prior large adapter bias is absent
in this pilot, but all-species equivalence is not yet established (comparator 3).
Direct saved-reference bridge is in `04-private/direct-parity`; it too has
inconclusive low-count species, with only one original realization per packet.
Missing direct-reference budget fields are not zero measurements and must not be
interpreted as budget discrepancies. Baseline paired budgets match exactly.

A larger installed/private comparison is running in `04-private/parity-16`,
using 16 chemistry seeds per packet and unchanged margins. The independent
physical sample size remains eight; chemistry replicas must be averaged within
packet before inference. Runtime finder instrumentation and source activation
implementation remain outstanding. This is not a blocker or physical NO-GO.

## Expanded baseline and trace preparation

The completed 256-history comparison (8 physical packets x 16 chemistry seeds x
2 installations) is recorded in `04-private/parity-16/comparison.txt` using
`Compare16.cc` (means within packet, t95 df7). Hydrated-electron difference is
+1.3672, CI95 [-0.6174,3.3518]. H2O2 difference -0.25, interval
[-1.5788,1.0788], margin 1.4941; HO2 difference +0.5156, interval
[-0.0988,1.1300], margin 1. Both remain narrowly inconclusive. No species is
classified different. A 64-replicate comparison is running in `parity-64`,
retaining the completed first 16 replicates. Physical sample size remains eight.

`FinderFixture.cc` adds a fixed H2O+ / hydrated-electron pair separated by 0.1 nm
at 1 ps. It compiles and runs with exit zero against unchanged private libraries;
its log is `04-private/fixed-fixture.log`. This alone does not diagnose the finder.
Separate `05-trace/source`, `build`, `install` are being built with model-init,
recombination finder flags/address, pre-sampling full engine state, probability,
candidate IDs/distances, and selected H2Ovib product logging. Source diffs are
saved in `05-trace/model-trace.diff` and `recombination-trace.diff`.
`FinderFixtureAdapter` is a diagnostic target for the rejected adapter, not an
unchanged baseline. No runtime trace conclusion or source-activation patch is
claimed yet. Both instrumented library and adapter builds are still in progress.

Fixed-pair diagnostic against unchanged private DLLs: native FinderFixture ends
with zero surviving tracks; FinderFixtureAdapter ends with surviving e_aq,
H3O+ and OH. Both exit zero. Adapter log records executable finder initially
unused, then the model enables the same executable-side address. This is runtime
behavioral divergence in a two-molecule fixture, not yet proof of DLL singleton
split. Evidence: `04-private/fixed-fixture.log` and
`05-trace/adapter-unchanged-runtime.log`. Both instrumented Geant4 translation
units compiled successfully; full trace-library build and parity-64 remain active.

## Demonstrated fixed-fixture cause

`05-trace/native-trace.log` and `adapter-trace.log` now establish a finder-state
split at runtime. Native model initialization and electron-hole recombination use
the same DLL-side instance (used=true; built=true at recombination). Adapter
initialization enables the executable-side instance, while DLL recombination
sees a different instance with used=false and built=false. Absolute addresses are
process-specific; identity comparisons are within each process.

Both full engine-state blocks match between runs, including immediately before
recombination sampling; both sample 0.60927. Native octree lookup returns electron
1000002 at internal distance 1e-7 mm (0.1 nm), recombination probability 0.99919,
and selects H2Ovib. Adapter takes the KD-tree branch and returns without a
candidate, retaining e_aq and producing H3O+/OH instead. This demonstrates a
cause of the fixed-pair divergence; it does not claim every ensemble discrepancy
has been independently attributed. Both runs exit zero. Module inventories
confirm instrumented G4processes_core.dll from 05-trace/build/BuildProducts/bin;
other dependencies come from that build or the preserved private installation,
with no configured shared Geant4 DLLs loaded.

The full trace install is still building. Expanded statistical comparison is
continuing with separate PowerShell worker processes (isolated runtime PATH),
retaining and validating prior per-process module inventories. Six independent
processes run concurrently; this is not a Geant4 MT-history acceptance test.

Final trace installation completed with exit zero. `RunFinderTrace.ps1` reran
both fixtures using exclusively `05-trace/install/bin` Geant4 DLLs, verified each
module inventory, identical reaction-table records and both complete engine
state blocks. `fixture-verification.txt` records PASS for those checks. Final
logs are `native-final.log` and `adapter-final.log`; the same finder split and
first-recombination divergence remain. This upgrades the earlier mixed-private
runtime diagnostic to a complete, independently installed instrumented runtime.

## Unchanged private baseline acceptance (bounded pilot)

The completed `04-private/parity-64/comparison.txt` reports ALL_SPECIES_EQUIVALENT 1
(exit 0): 8 independent physical packet clusters, 64 chemistry seeds per packet
per library, 1024 process histories. Margins remain max(5% baseline mean,1 molecule).
H2O2 delta +0.03125, CI95 [-0.74045,0.80295], margin 1.44912; HO2 delta +0.15039,
CI95 [-0.28846,0.58924], margin 1; e_aq delta +0.48047, CI95 [-0.67798,1.63892],
margin 201.16465. All species and total populations pass the existing marginal
pilot checks. No familywise coverage or production accuracy approval is claimed.
Paired energy, mass and source counts agree. Comparator source is Compare64.cc.
The original direct-reference bridge remains limited by its one realization per
packet; this acceptance is specifically private versus installed-library parity.

A candidate DLL source-contact patch is now building in separate `06-activation`.
It adds DLL-owned per-thread source registration/reset, same-time descendant
recognition, and one contact trial per source-related pair per activation time.
Original startup behavior and unrelated old-old contact rules remain unchanged.
Drivers bind the new API only under PROBE_PRIVATE_ACTIVATION. This candidate is
not accepted: scheduler boundary clock preservation, bulk/scoring continuity,
full fixtures, chemistry validation and independent Geant4 MT histories remain
outstanding. No ticket completion or commit. The scheduler currently clears
pending reactions when truncating a step at its stop boundary; do not describe
this contact-only candidate as implementing the entire proposed source contract.

Candidate build progress: patched G4DNAIndependentReactionTimeStepper.cc compiled
successfully. Runtime candidate controls are prepared in
`06-activation/RunCandidateControls.ps1` for startup, shifted startup, delayed
new-new, nonreacting populations and near-start new-old fixtures (three seeds).
They have not run yet. New-old activation is 1.001 ps, preserving actual diffusion
rather than freezing species diffusion coefficients. Existing SourceContacts
contact counters instrument only the rejected executable adapter; their zero
values in the private-library driver must not be interpreted as measured private
contact counts. Cross-reaction/timing assertions use actual scheduler callbacks.

Candidate library build/install and probe build completed successfully.
`06-activation/contact-result.txt` records all 15 controls passing: simultaneous,
shifted startup, delayed new-new, nonreacting population accounting and new-old,
three seeds each. New-old trace shows the first electron present at 1 ps, arrival
of the second at 1.001 ps, and their cross-packet reaction at 1.001 ps with no
premature activation. Candidate chemistry time-origin equivalence is now running
in `06-activation/invariance-16` (8 clusters x 16 seeds x two conditions). Runtime
module checks require every G4 DLL to come from the candidate installation.
This is validated contact-control scope only, not ticket 2 acceptance.

The first candidate is rejected by `interrupted-v1.log` (exit 2): simultaneous
ion/electron recombination causes the delayed contacting electron pair to remain
unreacted. Its partial invariance-16 ensemble was stopped and is NOT a completed
statistical test. The v1 DLL and source are preserved under `06-activation/v1`.
Review found two defects in the added trial-cache path: retaining trials after
native reaction-schedule invalidation, and std::minmax references to temporary
track-ID return values. Therefore the failed fixture alone does not isolate the
relative contribution of those two defects. Candidate v2 removes that cache
entirely and leaves repeat scheduling to the native algorithm. Its incremental
build is underway; no v2 success is claimed before rerunning controls.

Candidate v2 rebuilt/installed with exit zero. It passes 24 bounded controls:
startup, shifted startup, delayed new-new, nonreacting, new-old, interrupted
scheduling, empty source and simultaneous union, each with three seeds.
`v2-interrupted-1729.log` now shows the expected pair reacting at 10 ps after the
competing recombination. Extra controls assert no reaction/one spectator for
empty input, and one same-packet reaction with four survivors for the union.
The v2 chemistry ensemble is separate (`invariance-v2-16`), still running.
A portable experimental patch is saved under
`patches/geant4/11.4.3/timed-injection/source-contacts-experimental.patch`;
`git apply --check` against the unchanged private source succeeds. This is not an
accepted production patch, and no completion commit has been made.

V2's completed 256-history time-origin comparison (8 clusters x 16 seeds x
2 conditions) is inconclusive, not biased under the predefined classification:
H2O2 delta -0.52344, CI95 [-3.41914,2.37227], margin 1.65039; HO2 and O- also
remain inconclusive. Other species pass, including e_aq delta 0 with CI95
[-3.08060,3.08060]. No species is classified different. Source energy, mass and
counts match. See `06-activation/invariance-v2-16/comparison.txt` (exit 3).
The same candidate is now being tested at 64 chemistry seeds per packet in
`invariance-v2-64`, retaining the first 16 results and unchanged margins. Twelve
separate processes run concurrently; this is not Geant4 MT acceptance. Sample
expansion is adaptive exploratory work; no sequential-testing or familywise
coverage guarantee is claimed.

Additional v2 controls: bulk O2 depletion marker stays at 75269 molecules across
1, 10 and 20 ps with the same scavenger object (three seeds). Two-contact batches
produce callback scores at 1 and 10 ps without losing the first count; near-end
contacts react at 19.999 ps (three seeds each). These validate controlled bulk
state and callback accumulation, not every production output/scoring product.
The controls are registered under CTest label `injection-private` in the private
candidate probe build. Existing installed-kernel delayed failures remain visible
under their original baseline label; they were not hidden with WILL_FAIL.

At 64 seeds per packet, v2's time-origin ensemble has all species equivalent
except HO2: delta -0.29492, CI95 [-1.01633,0.42649], margin 1. This is narrowly
inconclusive, not different. H2O2 now passes: delta -0.19141, CI95
[-1.24676,0.86395], margin 1.64336. Sampling is being expanded to 128 seeds per
packet with the same eight clusters and unchanged margins; the 64-seed outputs
are retained. See invariance-v2-64/comparison.txt (exit 3).

Targeted descendant fixture: the initial draft incorrectly requested hydroxide
(user ID OH) rather than hydroxyl radical, so its zero-result logs are not
valid tests of the intended fixture. Corrected radical-ID runs are separately
named `v2-descendant-radical-*`. Across 32 seeds, 20 cross-packet reactions involve
a descendant at exactly its source activation time; later diffusion reactions
do not increment this counter. The old radical is present from 1 ps, while the
ion/electron source activates at 10 ps. Parent IDs are read through the native
track/IT ancestry. Existing independent contact-pair preservation is checked in
every run. This establishes bounded same-time descendant interaction evidence.

Real schedule work exposed another activation-boundary requirement. The new
ScheduledHistory driver uses the existing deposited-energy calibration and
Poisson sampler, samples whole library packets without translating them, and
adds each packet's actual molecular age to its primary-arrival time. Empty
histories remain empty, dose is computed from deposited energy, and no
retrospective dose rescaling/resampling is used. Resource bound: at most eight
packets and a 1 ns observation endpoint; exceeding it fails explicitly.

The first multi-arrival runs aborted with TIME_DONT_MATCH in the molecule
counter: BuildTrack counts future tracks immediately, advancing counter time
before earlier chemistry. Failed artifacts are preserved in `06-activation/schedules`.
Source inspection verified this behavior in G4Molecule.cc. The driver now pauses
counting only during construction of inactive tracks, restores the original
active state before chemistry, and registers every source track exactly once in
NewStage at its actual activation. No old counter/bulk reset is introduced.
The first failed schedule is being rerun in `schedules-v2`; no completed
multi-arrival success is claimed yet. Subsequent builds additionally compare
final BasicCounter species populations with the actual surviving track list.

The v2 time-origin comparison at 128 chemistry seeds per packet is complete:
`invariance-v2-128/comparison.txt` reports ALL_SPECIES_EQUIVALENT 1, exit 0.
There are still eight independent physical packet clusters (2048 process runs).
This validates the bounded natural-start versus distant-spectator activation
comparison under the unchanged exploratory margins, not the full source contract.

The corrected real Poisson schedule seed90002 completes with three selected
packets, arrivals 5.23764, 88.66591 and 89.60633 ps, 31423 source registrations,
102 ps endpoint, deposited dose 3.75330e-7 Gy versus nominal 1e-7 Gy, and about
229 s elapsed under concurrent load. The higher sampled dose is retained.
Library packet 1 was selected twice at exactly its stored coordinates; the 231
cross-ancestry reactions are mechanism evidence, not an unbiased physical-beam
cross-reaction estimate. Finite-library spatial reuse needs separate validation.

Subsequent `schedules-v3` runs include a final BasicCounter versus live-species
check. Empty seed90001, one-packet seed90005 and multi-packet seed90003 completed
with exit zero. Actual candidate DLL inventories and sampled peak working sets
are saved alongside their outputs. Counters remain active during chemistry;
only inactive-track construction is excluded, with registration at activation.
Native Geant4 Serial/MT complete-history isolation testing is now being built;
no MT result is claimed yet.

Native Geant4 history-isolation harness: 16 complete synthetic histories in
Serial and 16 on two MT workers, each across two BeamOn runs, all passed. Every
history reuses source IDs, registers five molecules across two delayed contact
batches, records two reactions and seven survivors, and checks BasicCounter
against live species. Deterministic event-dependent arrival offsets and exact
controlled budgets match between Serial and MT; chemistry trajectory equality
is not used as a general acceptance criterion. A repeat MT run captures actual
DLL paths exclusively under the candidate install. Files: histories-0,
histories-2, histories-2-verify and mt-result.txt. This is complete synthetic
history isolation, not yet realistic multi-packet beam validation in MT.

## Reproducible bounded validation commands

Paths below denote the resolved repository and its session scratch root; no
shared installation is modified. Configure the private probe against
`06-activation/install/lib/cmake/Geant4`, with `PROBE_PRIVATE_ACTIVATION=ON`,
the resolved vcpkg toolchain, and optional `PROBE_PACKET_LIBRARY` pointing to
`01-msvc/validation`. Put **only the selected private Geant4 bin first** on PATH
and retain required dependency/data paths. Inspect loaded modules as recorded
in the supplied run inventories; CMake linkage alone is insufficient evidence.

```powershell
ctest --test-dir <private-probe> -L '^injection-private$' --output-on-failure
cmake -DEXE=<private-probe>/IndependentHistories.exe `
  -DMACRO=<repo>/test/timed-injection/probe/Capture.in `
  -DOUT=<new-artifact-directory> `
  -P <repo>/test/timed-injection/probe/RunIndependentHistories.cmake
# Add -DLIBRARY=<scratch>/01-msvc/validation for complete packet histories.
<private-probe>/ScheduledHistory.exe <repo>/test/timed-injection/probe/Capture.in `
  <scratch>/01-msvc/validation <new-output-file> 90002 1000 100 102
```

The current contact suite was rerun after driver changes: 33/33, exit 0,
`06-activation/private-ctest-final.log`. The portable patch was applied in a
separate initialized scratch repository and the resulting two source files
compared with the compiled v2 source (normalized line endings):
`patch-verification.txt`, `validated-v2-hashes.csv`. This supersedes an earlier
subdirectory-only apply check that was not sufficient to establish applicability.

The final scheduled-history output includes species populations at existing
activation stages and 102 ps, without inserting extra scheduler stops.
`schedules-v4/seed90002.txt` contains all yields, arrival/selection/chemistry seeds,
31423 source registrations, 0.2928285644798389 MeV and 3.7533046702188814e-7 Gy.
Measured elapsed time was 170.280 s; sampled peak working set 329449472 bytes.
The single-packet seed90005 run took 18.689 s and 113782784 bytes. These are
observed process costs under concurrent load, not scaling estimates. Species
sample timestamps are chemistry times; final packet energy does not define a
continuous physical deposition curve.

Library reuse is explicit: seed90002 draws packet 1 twice and packet 5 once,
so three arrivals contain only two independent physical packets. At three
uniform draws from eight, duplicate probability is 1-(8*7*6)/(8^3)=0.34375.
Identical spatial coordinates on reuse can inflate inter-packet overlap.
Neither additional chemistry seeds nor additional histories remove this
finite-library spatial effect. Cross-ancestry counts from these schedules
are mechanism diagnostics, not an unbiased prediction for a continuously
irradiated independent beam.

## Complete packet histories in native MT

The extended IndependentHistories harness ran the same four source histories
in Serial and two-worker MT: seeds 90001 (empty), 90002 (three packets), 90003
(two packets) and 90005 (one packet), across two BeamOn runs. Each worker owns
its entire chemistry history, counters and activation registry. Every run exits
zero; all eight histories pass actual counter/live-population checks. Source
IDs are reused only after a complete-history reset. Source schedules, counts,
deposited energy and dose match exactly across worker modes; stochastic reaction
and yield differences are not rejected by trajectory equality.

Artifacts: `06-activation/packet-histories-0`, `packet-histories-2`, and
`packet-mt-result.txt`. Both processes' actual Geant4 module paths are exclusively
under the candidate private installation. Seed90003 uses distinct packets 5 and
3 at 28.932058 and 62.016758 ps, registering 22151 precursors and retaining its
0.20658934440566767 MeV deposited-energy budget. These are bounded history/state
checks, not a powered Serial-versus-MT chemistry distribution comparison.

`RunIndependentHistories.cmake` reproduces the native Serial/MT source checks;
without LIBRARY it runs the synthetic 16-history cases. That synthetic script
was rerun successfully against the updated driver (`histories-final.log`).
Optional `PROBE_PACKET_LIBRARY` registers the real-packet CTest separately under
`injection-private-packets`; synthetic complete histories use
`injection-private-histories`. The 33 short contact controls retain their own
`injection-private` label.

The tracked CompareInjectionResults now accepts an optional chemistry-replica
count (default two), retaining exactly eight independent physical clusters and
t95(df7). Reanalysis at 64 and 128 replicas reproduces the archived fixed-count
comparators' outputs byte for byte. Missing direct-reference energy fields are
not physical zero measurements: direct precursor hashes and energy files carry
that evidence; only measured species enter chemical equivalence classification.

## Fresh direct-generation reference and final verified state

The saved direct realization is preserved; it is no longer the only direct
comparison. `04-private/RunDirect64.ps1` and `DirectWorker.ps1` run the original
installed TrackCapture direct driver at the original eight physical/source seed
pairs, with 64 chemistry seeds per packet matching the private baseline ensemble.
All 512 newly generated precursor files have the same SHA256 as their respective
original library packet. Actual loaded DLL paths are checked against the shared
installed runtime in these read-only reference runs. Outputs are separate under
`04-private/direct-fresh-64`; no original reference was overwritten.

The fresh direct-versus-unchanged-private comparison reports
ALL_SPECIES_EQUIVALENT 1 (exit 0). Chemical replicas are averaged within each of
eight independent physical packets; t95(df7) and max(5% mean, one molecule)
exploratory margins are unchanged. H2O2 difference is 0.183594 with interval
[-0.628407,0.995595] and margin 1.441504; HO2 difference -0.181641 with interval
[-0.588181,0.224900] and margin 1. The earlier single-realization rare-species
precision limitation is resolved for this bounded pilot comparison. The eight
physical clusters, adaptive sampling history and lack of familywise coverage
still limit interpretation; this is not production accuracy certification.
The tracked general comparator independently reproduces the successful result.

Final current-driver regression: `06-activation/final-regression.log`, 34/34
checks, exit 0 (33 contact controls plus complete synthetic Serial/MT histories).
Real packet Serial/MT evidence is separately in `packet-mt-result.txt`, with all
four schedules per mode passing. Branch and HEAD remain
`wip/precursor-capture-probe`, `84fca374a426788319d514437c443b22979d001a`.
No completion commit, PR, shared Geant4 edit or full-dose simulation was made.

### Acceptance boundary and ticket 3 readiness

The private unchanged baseline, demonstrated fixed-fixture finder cause, bounded
source-contact implementation, direct/import chemistry comparison, activation
invariants, controlled bulk/counter continuity and complete Serial/MT history
isolation now have executed runtime evidence. These are implemented results,
not an outstanding private-build plan or a claim of physical infeasibility.

Ticket 2 remains InProgress, not accepted as a general recurring-beam reference.
Remaining work is broader than the validated contact patch: independent-packet
spatial reuse sensitivity, a powered chemistry comparison for realistic combined
cloud/schedule controls, and production scoring continuity beyond the counters
and callback scores exercised here. The stricter proposed kernel contract also
requires an explicit decision/implementation for unaffected pending clocks:
native IRT_syn schedule rebuilding is retained and was not replaced by a
transactional source-batch operation. Current evidence must not be relabelled
as verification of that stronger contract. These are unfinished acceptance
items, not an environmental blocker or evidence of physical infeasibility.
Ticket 3 remains gated; no mesoscopic coupling/scaling result is claimed.

## Continued acceptance work, 2026-11-08

The preceding status is historical. Ticket 2 does not require identical sampled
reaction trajectories, preservation of each pending random reaction clock, or
integration into the production scorer. It does require correct activation,
retained old chemistry and bulk state, independent complete histories, and
statistically credible bounded chemistry controls. Native IRT_syn schedule
rebuilding remains part of the candidate; a distant-source boundary experiment
tests its effect rather than assuming that effect is zero.

`CombinedHistory` imports each of eight independent `(library_i, holdout_i)`
packet pairs. The simultaneous two-label and combined-cloud controls construct
the same molecules in the same order with the same IDs, positions and times;
only diagnostic ancestry labels differ. The delayed control places the second
primary at 1.5 ps, activates its 1 ps precursors at 2.5 ps, and observes chemistry
through absolute 3 ps. This is still a short particle reference. A separate
duplicate-packet control deliberately repeats identical spatial coordinates;
it measures an extreme finite-library reuse effect, not an independently
sampled beam.

Live population snapshots are taken at existing native activation stages and
the endpoint; scoring adds no scheduler stops. The cumulative deposited-energy
ledger uses primary arrival times, separately from precursor activation times.
For pair 1 the delayed fixture registers 17622 precursors: the 1 ps snapshot has
0.07669770016825485 MeV, and the 2.5 ps and 3 ps snapshots have
0.16435537174827888 MeV. The pre-primary zero is explicitly labelled `0_minus`.
Energy is a per-packet deposition ledger, not a reconstructed sub-track physical
deposition-time curve.

The project's BasicCounter intentionally ignores the water molecule definition,
including excited and ionized configurations. Immediate stage counter checks
therefore exclude those definitions while chemistry snapshots retain them.
GetRecordedMolecules reflects the current counter map; it is not lazily finalized.
The project's 10 ps counter comparison precision precludes interpreting later
queries as resolved retrospective 1 ps histories. Stage checks query immediately.
An earlier failed assertion and a separate heap-failure output remain preserved
in the smoke artifacts; the heap failure has not been assigned a causal explanation.
The corrected eight-pair, three-condition smoke completed 24/24 histories.

Diagnostic ancestry now preserves the union of both reaction parents. Reactions
between nonempty disjoint masks are reported separately from reactions involving
overlapping mixed ancestry. The focused mixed-product follow-up check exercises
this bookkeeping without changing reaction selection. The batch runner rejects
resumes if the executable, private Geant4 DLLs, macro, packet inputs or runner
scripts differ from the saved manifest. Positive unchanged-resume and negative
changed/missing-manifest tests were executed.

The current candidate `injection-reference` CTest suite passed 36/36 checks:
33 private source controls, the mixed-ancestry check, complete synthetic Serial/MT
histories, and the real simultaneous/combined/delayed control. Log:
`07-reference-ctest.log` under the session scratch root. The original installed
kernel diagnostics remain available in nonprivate builds; they are not converted
to expected failures to make candidate acceptance pass.

The reuse pilot completed all 32 histories: eight independent physical packet
pairs, two chemistry replicas, distinct versus identical-coordinate reuse.
Results are normalized by each history's deposited MeV before averaging within
physical pair; confidence intervals use the eight pair means and t95(df7).
H2O2 rises from 357.44 to 898.96 molecules/MeV (difference 541.53,
95% interval [473.47,609.58]). Disjoint cross-ancestry reactions rise from zero
observed to 1529.82/MeV (difference interval [1417.91,1641.73]); subsequent
overlapping mixed-ancestry reactions also occur. Artifact:
`07-reuse-pilot/reuse-comparison.txt`. Two chemistry replicas give only limited
within-pair precision; the cluster variation contains residual chemistry noise.
Zero observed cross reactions in the distinct sample does not establish zero
cross-reaction probability in general.

This is a demonstrated spatial-library reuse effect, not failed equivalence:
the two physical source clouds are intentionally different and were never
required to be equivalent. Repeated chemistry seeds cannot remove this bias.
An eight-packet library reused without spatial diversification is therefore
unsuitable for an unbiased independent-beam prediction. The bounded reference
can still test identical, explicitly specified source histories; production
beam extrapolation is not validated by this study.

The combined ensemble completed 384 histories: eight independent physical
packet pairs, 16 chemistry replicas and three conditions. Its separate audit
found zero errors in all histories and 128 grouping pairs: source counts,
registrations, exact energy sums, dose ledgers, early delayed snapshots and
nonempty candidate-only DLL inventories. Conditions 0/1 pass all observed
species and total-population equivalence checks. H2O2 difference is 0.47656
molecules per pair, 95% interval [-2.18933,3.14245], margin 3.49883; HO2 radical
difference is -0.33594, interval [-2.05834,1.38646], margin 2.17344. Chemistry
paths differ despite paired seeds; exact path equality was not an acceptance
condition. Artifacts: `07-combined-16/comparison.txt` and `budget-verification.txt`.

The distant-source boundary experiment completed 2048 histories: eight physical
packets, 128 chemistry replicas and two arms. An O2 spectator is inserted at
1.5 ps, at least 158.125 um from any initial precursor, while the old packet is
already active. The old packet's deposited energy and source count are matched;
the spectator's single O2 is subtracted from endpoint yields. No cross-ancestry
reaction is allowed, and spectator survival is checked. The 16-replica pilot was
inconclusive for H2O2 and HO2; expansion to the predefined 128-replica endpoint
resolves both under the unchanged rule max(5% baseline mean, one molecule).
H2O2 difference is -0.32813, 95% interval [-0.89973,0.24348], margin 1.64741;
HO2 difference is -0.11816, interval [-0.43440,0.19807], margin 1. All observed
species and total population meet the exploratory margins. Artifact:
`06-activation/boundary-128/comparison.txt`.
The independent audit checks all 2048 histories with zero budget errors and
all 2048 nonempty module inventories with zero wrong paths. Per-packet deltas
and within-packet variability for both sample sizes are recorded in
`06-activation/boundary-cluster-audit.txt`. Exact ensemble executables are
preserved beside their results. After the diagnostic ancestry correction,
the current source was rebuilt and the combined control passed again; that
correction changes the legacy fixture's diagnostic callback, not the
HistoryAction callback used by these ensembles.

These are marginal cluster-based intervals, with eight independent physical
units and chemistry replicates averaged within each. The boundary expansion
retains its pilot and is exploratory rather than an independent confirmatory
test. Absence of an observed rare event is not proof of zero event probability.
The controls support a short, explicitly scheduled particle reference; they
do not certify arbitrary irradiation durations or full-dose beam accuracy.
Final evidence audits pass and independent review found no unresolved actionable
code findings. The corrected current-source combined control and paired boundary
smoke both pass after rebuilding. The mixed-ancestry unit fixture exercises the
merge helper rather than the complete reaction callback; callback propagation
was reviewed, and realistic cross-ancestry evidence uses the separate full mask
propagation in HistoryAction. Archived ensemble binaries are preserved, but the
earlier runs lack a contemporaneous complete source-hash manifest; current-source
controls mitigate this provenance limit, and new runs enforce manifests.

**Readiness decision:** the bounded, explicitly scheduled particle reference is
credible for ticket 3's matched coupling comparisons. Activation, packet energy,
old-population/bulk continuity, diagnostic ancestry, scoring and complete-history
worker isolation have executed controls; chemistry comparisons use statistical
equivalence rather than identical trajectories. This does not approve an
untransformed eight-packet library for independent-beam predictions, arbitrary
source rates, seconds-long chemistry, or production use. Ticket 3 must establish
its own coupling correctness, convergence and scaling, and may conclude NO-GO.

### Reproducing the private reference

Resolve the source and install configuration using `.claude/.claude-project.json`
and the project configuration-resolution instructions. Use an isolated copy of
the configured Geant4 source, a separate build directory and a separate install
prefix. First build the unchanged copy with the configured installation's
options; the executed option inventory and commands are preserved in
`04-private/options.cmake`, `shared-cache.txt` and `build-command.txt`. For the
candidate, apply `patches/geant4/11.4.3/timed-injection/source-contacts-experimental.patch`
to a second isolated source copy with `git apply --check` followed by `git apply`.
Rebuild and install only into that copy's private prefix. The corresponding
executed candidate configuration is in `06-activation/options.cmake` and
`build-command.txt`. Do not apply the patch to the shared source or installation.

From an MSVC x64 developer shell, with `$privateInstall`, `$probeBuild` and
`$packetLibrary` set to those private paths and the preserved ticket-1 validation
directory:

```powershell
cmake -S test/timed-injection/probe -B $probeBuild -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  "-DCMAKE_TOOLCHAIN_FILE=$env:DEV_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  "-DGeant4_DIR=$privateInstall/lib/cmake/Geant4" `
  -DPROBE_PRIVATE_ACTIVATION=ON "-DPROBE_PACKET_LIBRARY=$packetLibrary"
cmake --build $probeBuild --parallel 6
$env:PATH="$privateInstall/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
ctest --test-dir $probeBuild -L injection-reference --output-on-failure --no-tests=error
```

Do not set `PROBE_G4_SOURCE` for this candidate build: that option enables the
historical rejected executable adapter. Always check actual loaded Geant4 DLL
paths, not merely PATH ordering. The PowerShell ensemble worker captures them.
Do not relink an executable while its histories are running.

For the existing session scratch layout, `RunCombined.ps1 -Root <session-scratch>
-Out <new-empty-directory> -Replicas 16 -Conditions '0,1,3' -Parallel 6` runs
the simultaneous two-label, union-label and delayed histories. A separate
two-replica run with `-Conditions '0,2'` reproduces the bounded reuse comparison.
`CompareInjectionResults <combined-output> 16` compares conditions 0/1;
`CompareCombinedReuse <reuse-output> 2` reports reuse sensitivity without an
equivalence requirement. Result names carry physical-pair, chemistry-replica
and condition indices. The runner verifies provenance before resuming.

`BoundaryHistory <Capture.in> <library_i.packet> <output> <seed> <0-or-1>`
runs the no-boundary or distant-spectator arm. The executed seed rule is
`81001 + i*101 + replica*1009`, with i=0..7 and replica=0..127, and output names
`p{i}r{replica}m{arm}.txt`. Use `CompareInjectionResults <output-directory> 128`
only after all 2048 complete histories and their exact budgets are verified.
This expansion retains the initial 16-replica pilot, rather than representing
an independent confirmatory sample. Runtime output belongs in scratch; packet
library, pilot and checkpoint evidence must not be overwritten.
