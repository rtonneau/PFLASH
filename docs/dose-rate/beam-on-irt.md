# Bounded beam-on IRT_syn reference

This is an isolated **beam-on feasibility reference**, not production
integration. At baseline `7b5b2bd1fff0698c7a4daf9905bcaf1e110b192c`, a
private Geant4 11.4.3 runtime combines the molecular-finder correction, native
source-contact activation and reflecting-boundary changes. A single native
IRT_syn history receives all scheduled sources and evolves their molecules
together. The tested regime is a 4 µm water cube, 500 keV electron precursor
packets, 0.002 Gy nominal dose and at most eight source arrivals per history.
It supports an exploratory bounded implementation GO for that regime. It does
not establish the cost or accuracy of a 0.5 mm, up-to-10 Gy irradiation,
production streaming, or any mesoscopic tail.

> **Later result (2026-11-09).** The calibration-precision gap left open here
> was addressed by one bounded fresh-source study (Study06: 1024 calibration
> captures per condition, 128 paired fresh realizations). It completed and was
> accepted locally, but **both 5% calibration targets were missed** (8.05% water,
> 8.01% oxygen), so its comparisons are exploratory. Consolidated result and
> limits: [beam-on-fresh-source-calibration.md](beam-on-fresh-source-calibration.md);
> next session: [handoff](beam-on-fresh-source-calibration-handoff.md). The
> Ticket04, Study05 and ledger-repair sections near the end of this page are
> incident history and are marked superseded where their status changed.

## Source and observation contract

`BeamSchedule` uses `lambda = doseRate * mass / (meanEdep * 1.602176634e-13)`
with MeV and kilograms, and samples exponential waits in a fixed, half-open
exposure window. Whole packet selection has its own random engine, separate
from arrivals and Geant4 chemistry. The source count is Poisson; the run does
not stop at a prescribed count or delivered dose. A ninth arrival is a censored
history, not a resampled one. Energy is booked at the primary arrival; each
precursor activates at that arrival plus its recorded 1 ps age. Consequently,
valid primaries just before beam end can activate after beam end. Native
observation is at exposure end + 2 ps, after these activations.

The driver preconstructs future source tracks into Geant4 delayed lists, with
distinct track IDs and source ancestry, and runs **one** particle scheduler.
Counter registration occurs at the observed activation; expected and observed
times, packet energy, actual dose and bulk counts are recorded. The actual
bulk material stays in the native history. Engineered depletion and native
contact tests exercise continuity. The bulk accessor reports Geant4's tracked
scavenger counts; its special water value is not a physical water inventory.
The end observation checks eligible live species against the native molecule
counter. Empty histories retain zero population and their unchanged oxygen
pool. The job peak includes preloaded future input as well as live chemistry;
it is not a measurement of live chemistry alone or evidence for lazy streaming.

## Results and interpretation

The predeclared [protocol](../../test/timed-injection/probe/BeamOnProtocol.md)
uses 1, 10, 100 and 1,000 Gy/s at the same nominal dose. Its empirical-bank
experiment uses four disjoint physical-packet clusters per oxygen condition,
two source/chemistry replicas per cluster and common random streams across
rates. All **64** histories passed. Rates have identical realized source
counts and energies within each paired realization, with fluctuating counts
and actual dose across realizations. Packet selection with replacement can
reuse an exact physical track at the same position within a history; prior
diagnostics show that this can bias inter-track chemistry. These 64 histories
are therefore finite-bank mechanism/reference results, not unbiased beam
estimates.

At 1 and 1,000 Gy/s, the oxygen-free empirical-bank mean final aqueous-electron
counts were 4.5 and 25.125 per history, respectively; their paired difference
was 20.625 with an exploratory t(3) interval of [8.76, 32.49] across four
physical clusters. This is a result for the stated delivery and observation
protocol, including its finite bank, changing exposure duration and molecular
ages. It does **not** identify the portion caused by inter-track interactions;
matched single-track controls would be needed for that attribution. The
reported intervals do not include finite-library bias or calibration error.

The additional fresh-per-arrival control used four independent Poisson source
realizations per oxygen condition at 100 and 1,000 Gy/s. Each of the ten
captured packets was used once within its realization, with the same fresh
realization paired across rates. All **16** histories and all captures passed;
two oxygenated realizations had no arrivals. Mean actual doses were 0.001881 Gy
for water and 0.000682 Gy for oxygenated water, paired identically across rates.
For water, the 1,000-minus-100 Gy/s paired final aqueous-electron count was
1.0 per history, t(3) interval [-2.44, 4.44]; the hydroxyl-radical difference
was 2.0, interval [-5.23, 9.23]. In oxygenated water the corresponding
hydroxyl-radical difference was -0.5, interval [-1.42, 0.42], and actual
oxygen-pool change differed by 0.5 molecules, interval [-0.42, 1.42]. These
are raw counts per history, not G values. No disjoint-source reactions were
observed in these 16 fresh histories; zero observed does not prove their
absence at other doses or rates. Four realizations give weak precision and do
not justify equivalence or a biological-effect claim.

For the fresh control, the fixed 4 µm calibration banks had mean deposits
0.000458386 MeV (SE 0.0000856205, 16 water packets) and 0.000556468 MeV
(SE 0.000166950, eight oxygenated packets), with mass `6.4e-14 kg`. The
relative standard errors are approximately 18.7% and 30.0%; they are not
included in the intervals above. The source means used for calibration were
fixed before fresh captures. In a fresh run the history driver's `mode fixed`
and zero `rate_s_inv` denote the externally generated schedule; the separately
recorded `.calibration` and schedule logs carry the actual Poisson calibration.
The history's packet mean describes those fresh inputs and is not used to
recalibrate their arrivals.

> **Context only (2026-11-09).** These four-realization intervals rest on
> calibration banks of 16 and eight packets (relative SE about 19% and 30%). The
> Study06 calibration (1024 captures per condition, 8% relative half-width) and
> 128 paired realizations per condition replace them as the fresh-source
> reference; do not pool the two. See
> [beam-on-fresh-source-calibration.md](beam-on-fresh-source-calibration.md).

All **18** compressed 2/4/8-source cost controls used distinct original
packets and passed; their short separations are mechanism stresses, not
physical-rate predictions. Their maximum job-reported private peaks were
12,992,512 / 13,631,488 / 16,773,120 bytes for water and 13,029,376 /
13,594,624 / 16,728,064 bytes for oxygen, respectively. Full process wall-time
ranges were 0.219–0.267 / 0.225–0.253 / 0.293–0.302 seconds for water and
0.215–0.226 / 0.212–0.233 / 0.278–0.290 seconds for oxygen. The largest
fresh-control process, including physical capture, peaked at 54,009,856 bytes
and took 8.84 s. These tiny fixtures do not establish a scaling law. Sixteen
fixed two-source controls with distinct captures and illustrative 1 and 10 µs
gaps also passed; they are fixed-count checks rather than Poisson evidence.
The history files' `init_seconds` ends before staging future tracks and
`chemistry_seconds` begins after staging. Their sum omits that staging work;
the wall times and peaks above cover the complete Job Object process.

## Validation and provenance

The composed runtime passed 55 capped finder, native coincidence,
recombination and retained-guard cases. It also passed 36 native source
controls, including depleted-bulk continuity and delayed zero-distance
contacts between distinct source IDs, plus the reflecting-boundary unit and
16 native boundary-activity processes covering all axes, sides and transport
and dissociation modes. Thirteen capped driver checks covered empty, single,
simultaneous, close, delayed and post-beam activations, explicit malformed,
incompatible and excluded-activation rejection, and source RNG invariance.
The two pure packet/schedule CTests passed. All native processes ran serially;
the finder suite used its stricter 256 MiB / 5 s cap, and the remaining
histories used 512 MiB committed-private memory / 60 s, with a 3 GiB free
system-memory floor. No final control or protocol history hit a cap. Loaded
Geant4 DLL paths and before/after hashes were checked. A separate ledger
audit verified all 114 protocol histories, activation IDs/times, dose sums,
source bounds, paired schedules and no within-history fresh-packet reuse. Its
negative controls rejected missing activations, altered energy, future
arrivals, missing registrations and a truncated ledger.

The installed and built private `G4processes_core.dll` SHA-256 is
`11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B`.
All 17,644 private source files were compared with the configured pristine
tree: exactly the nine files owned by the three patches differed. An
independent reconstruction from pristine bytes matched all nine. Protected
shared source/install and patch hashes showed no drift. The first composition
attempt silently skipped the `diff --git` source-contact patch because the
copied source was still nested inside the parent Git worktree. Its build
failed the native activation API compile; the incomplete core
`56000465FAA6EAEA09E7E53A6DD10CFC9764C2886A0118507DA942F7DE932267`,
logs and seven-file comparison are preserved in `pre-source-contact-build`.
A scratch-private Git boundary, verbose patch checks and the nine-file
reconstruction resolved this; the earlier core is not the validated runtime.
The original finder-only, timed-injection and rejected copied-adapter evidence
was not overwritten or counted as acceptance for the composed runtime.

All raw local evidence is under
`.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso`: `finder-validation`,
`runtime-validation`, `beam-checks`, `fresh-protocol`, `empirical-protocol`,
`protocol-audit.json`, source comparisons and build logs. These ignored files
must be archived for independent reproduction. The fresh run used the exact
protocol bytes preserved in `fresh-protocol/protocol-at-run.md`; a later prose
clarification about interpreting delivery-rate comparisons changed no numeric
settings and is reflected in the tracked protocol and empirical run.

## Reproduction

Independent code and scientific review identified one analysis omission:
paired contrasts originally enumerated only species present at the higher
rate. Both analyzers now use the union of both arms, with absent species
counted as zero, and require both arms to be complete. A synthetic regression
checks baseline-only and comparison-only species, including the paired means
and t3 intervals. It failed before the correction and passed afterward.
Reanalysis preserved every original history and summary: corrected files are
`fresh-protocol/analysis-review-v3.txt` and
`empirical-protocol/analysis-review-v2.txt`. Fresh results are unchanged;
the empirical summary gains four previously omitted contrasts. All previously
reported numerical contrasts remain unchanged.

Run the analysis regression with a new output directory:

```powershell
./test/timed-injection/probe/TestBeamOnAnalysis.ps1 -OutDir "$task/recheck-analysis"
```

Resolve the configured source, install and vcpkg paths through
`.claude/.claude-project.json` and `~/.claude/CONFIG_RESOLUTION.md`. Start from
a new private copy of the configured source, excluding `.git`. Initialise a
Git boundary **inside that copy**, then apply in order
`patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch`,
`patches/geant4/11.4.3/timed-injection/source-contacts-experimental.patch`,
and `patches/geant4/11.4.3/timed-injection/meso-boundaries-experimental.patch`
with `git -C <private-source> -c core.autocrlf=false apply --verbose`. Confirm
all nine paths were actually applied and independently compare the resulting
bytes with pristine-plus-patches; exit status alone was insufficient here.

The local exact capped build commands are preserved in the evidence directory
as `Build.cmd` and `BuildProbesV3.cmd`, and job metadata in
`kernel-build-v4-contact.meta` and `probes-build-v3.meta`. They use MSVC x64,
Release/Ninja, four workers, the configured vcpkg toolchain, system zlib and
HDF5, and no Geant4 data download. The build/install job uses a 4 GiB
per-process / one-hour cap. Compile the existing `test/molecular-finder/JobLauncher.cc`
with `psapi.lib` to provide that Job Object launcher. Configure the probe with
`PROBE_PRIVATE_ACTIVATION=ON`, `PROBE_MESO_EXPERIMENT=ON` and
`PROBE_MESO_PRIVATE_BIN=<private-install>/bin`, and build only
`BeamOnHistory`, `InjectionReference`, `FiniteScheduleTest`, `TrackPacketTest`,
`MesoBoundaryTest`, `MesoBoundaryIntegration`, `FreshSchedule` and
`TrackCapture4um`. The `PROBE_MESO_EXPERIMENT` option supplies existing 4 µm
capture and reflecting sources; unrelated mesoscopic targets require other
instrumentation and were not built or claimed. Do not enable `PROBE_G4_SOURCE`:
that selects the previously rejected copied-adapter diagnostic.

With `$task` the new scratch root, `$prior` the retained
`.scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/08-meso`
and the private bin plus vcpkg bin first on `PATH`, run:

```powershell
ctest --test-dir "$task/probe" -R '^(FiniteScheduleTest|PacketAndSource)$' --output-on-failure --no-tests=error
./test/molecular-finder/Validate.ps1 -BuildDir "$task/finder-probe" -InstallPrefix "$task/install" -OutDir "$task/recheck-finder"
./test/timed-injection/probe/ValidateBeamOnRuntime.ps1 -BuildDir "$task/probe" -FinderBuildDir "$task/finder-probe" -InstallPrefix "$task/install" -OutDir "$task/recheck-runtime"
./test/timed-injection/probe/CheckBeamOn.ps1 -Exe "$task/probe/BeamOnHistory.exe" -Launcher "$task/finder-probe/JobLauncher.exe" -InstallPrefix "$task/install" -Library "$prior/library4um-16" -OutDir "$task/recheck-schedules"
./test/timed-injection/probe/RunFreshBeamOn.ps1 -BuildDir "$task/probe" -FinderBuildDir "$task/finder-probe" -InstallPrefix "$task/install" -WaterLibrary "$prior/library4um-16" -OxygenLibrary "$prior/library4um-oxygen-8" -OutDir "$task/recheck-fresh"
./test/timed-injection/probe/RunBeamOn.ps1 -BuildDir "$task/probe" -FinderBuildDir "$task/finder-probe" -InstallPrefix "$task/install" -WaterLibrary "$prior/library4um-16" -OxygenLibrary "$prior/library4um-oxygen-8" -OutDir "$task/recheck-empirical"
./test/timed-injection/probe/AnalyzeFreshBeamOn.ps1 -EvidenceDir "$task/recheck-fresh"
./test/timed-injection/probe/AnalyzeBeamOn.ps1 -EvidenceDir "$task/recheck-empirical"
```

Each runner refuses to overwrite an existing evidence directory. Inspect all
individual `.meta`, `.log`, `.err`, `.modules`, `.args`, source ledgers and
hashes, including any failed or censored run; a launcher exit by itself is
not a successful history. The fresh schedules use the fixed calibration and
seeds in the protocol, and each capture records its own physical, source and
chemistry seeds. Capture banks are local ignored evidence and depend on the
exact project-source fingerprint and macro identity; do not substitute a
different geometry, environment or source-version bank.

## Ticket04 bounded execution — interrupted, 2026-11-08

> **Superseded as a status (2026-11-09).** Study04 stays excluded from every
> statistic, and the overwritten `03-native-controls` files are not immutable
> originals. "Parent review is required before any separately authorized future
> study" and the "Ticket04 … remain incomplete" statements below are resolved:
> Ticket04 was completed by Study06 and accepted locally
> ([result](beam-on-fresh-source-calibration.md)).

The single authorized study was launched from baseline
`2c3c7ed696820345b86cf19b0655d8e2bdb185b5`, but was stopped after an operator
preflight output-isolation error was discovered. It is an incomplete, invalid
execution for acceptance; no replacement study was started. This is not a
four-hour deadline result. Parent review is required before any separately
authorized future study.

Evidence root: `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration`.
Fresh `04-preflight` process logs passed three native process/provenance audits
and `TestCalibratedBeamOn.ps1` Debug checks printed
`CALIBRATED_BEAM_ON_CHECKS_PASS`. However, slash-sensitive string replacement in
the operator's control command failed to redirect packet/history arguments:
those outputs were written into `03-native-controls`. Earlier control outputs
were therefore overwritten; they must not be represented as immutable original
evidence. Fresh `.args` files expose the error. No production code was changed.
Inventoried files match recorded hashes; this does not establish absence of intervening writes or unlisted changes.

Actual study command (paths relative to the repository):

```powershell
./test/timed-injection/probe/RunCalibratedBeamOn.ps1 `
 -BuildDir .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/03-probe-release `
 -FinderBuildDir .scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/finder-probe `
 -InstallPrefix .scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install `
 -OutDir .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/04-study
```

Study start was `2026-11-08T14:57:33.4856071Z`; the original deadline was
`2026-11-08T18:57:33.4856071Z`. Operator interruption occurred before the
14:58:35Z post-stop analysis. Raw `processes.csv` preserves 12 accepted capture
jobs, one attempted but not runner-adjudicated capture, and 5367 unexecuted
jobs. These include reserved fresh slots and analysis jobs, not 5367 missing
observations. All 13 physical outputs are retained. The final attempt's native
process audit passes, but it is excluded from accepted statistics because
runner reconciliation did not finish. No retries, schedules or chemistry
histories occurred. Water holdout and all oxygen captures remain unexecuted.
No full ensemble or zero-retention population claim follows from these survivors.

Original post-stop analyzer files are retained in each condition's
`analysis.summary.tsv`; both report `complete=0`, `propagation_supported=0`.
The 12 accepted water captures give descriptive mean 0.0008840510571 MeV,
SD 0.0008963804524, SE 0.0002587627477, zero count 0 and relative t half-width
64.423%. This is not the fixed-1024 precision test: that target was not evaluated.
Holdout comparisons, conditional chemistry intervals, combined calibration design
sensitivity, ESS and overflow-support acceptance are unavailable. No biological,
full-dose or equivalence conclusion is supported.

`04-audit` contains separate identities/process audits and independent hash,
seed, energy and state checks. No activations or paired schedules exist to audit.
`04-preflight/hashes.csv`, study `hashes-before.csv`, `manifest-hashes.csv`,
`input-snapshots.csv` and `input-bytes/` preserve exact hashes and input bytes;
`protocol-at-run.md` is the untouched protocol snapshot. Core SHA256 remains
`11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B`.
The nine changed private source paths matched both pristine and patched hashes
in the retained provenance record (`04-preflight/source-check.txt`).
`04-audit/archive-manifest.csv` inventories ignored ticket04 outputs for archival.
The interrupted runner did not produce its normal final status/hash files;
independent post-stop hash comparisons are explicitly separate evidence.

The independent incident review found byte-identical 04-preflight and
03-native-controls argument files. Exactly these five paths under
`.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/03-native-controls/`
were rewritten: `control-capture.packet`, `control-capture.energy`,
`control-capture.chemistry`, `control-100.history`, and
`control-1000.history`. No pre-overwrite hashes were found. Identical seeds
do not certify original bytes; those original bytes remain uncertain.
The inventories cover 39 preflight, 242 input, 16 manifest, 651 prior and
3997 archive files, with 242 input snapshots. Inventoried files match recorded
hashes, with no claim of exhaustive absence of writes.

Prevention is available as the read-only planning interface below:
```powershell
. ./test/timed-injection/probe/CalibratedPreflight.ps1
$plan = New-CalibratedPreflightPlan -ControlDir "$task/new-controls" -Macro ./test/timed-injection/probe/MesoFixture4um.in
$plan | ConvertTo-Json -Depth 6
Assert-PreflightDestinations $plan.ControlDir $plan.Destinations
```
This plans all three native argument arrays plus packet, energy, chemistry,
history, process sidecars, audit, schedule and manifest destinations before
any creation. It rejects existing destinations, duplicates, escaping paths
and existing reparse-point ancestors, and requires a new control directory.
No serialized previous arguments are accepted. A future authorized caller
must consume these exact Jobs.Arguments arrays and destinations, revalidate
immediately before creation, and retain exclusive ownership of the directory.
Validation is read-only, not an atomic filesystem reservation against other
writers. Manifest/schedule contents and runtime provenance still require
the established controls; this helper launches nothing.

The existing runner rejects an existing OutDir and has no resume mode.
The original absolute deadline 2026-11-08T18:57:33.4856071Z cannot reset.
Any new study requires separate authorization and declaration. Ticket04
and the full study remain incomplete; no scientific inference follows.

## Authorized 05-study terminal outcome — 2026-11-08

> **Superseded as a status (2026-11-09).** Study05 is closed and excluded, kept
> here as incident history. "Ticket04 acceptance remains with the parent" was
> resolved on the basis of Study06, not Study05
> ([result](beam-on-fresh-source-calibration.md)).

Launched exactly once at 2026-11-08T20:09:40.6845574Z with the approved new budget. Runner exited 1 after 109.2153426 seconds: RunCalibratedBeamOn.ps1 line 95, Move-Item replacing processes.csv reported "Cannot create a file when that file already exists." No retry, replacement, source modification or further sampling followed. The monitoring reader accessed processes.csv during execution; whether file sharing caused the replacement failure is unresolved. The finally block persisted status and after-hashes. Its empty stop_reason is not a success indication: the uncaught exception is the terminal cause.

65 accepted water calibration captures completed, including one complete zero deposit. The runner counted 66 attempts: slot water-capture65 was marked attempted before Save failed, has no native log/meta/energy output, and its capture audit rejected missing evidence. Thus 65 actual native captures, one rejected prelaunch slot, 4542 registry-unexecuted capture slots; zero censored native processes. All 512 histories, 256 schedules and four in-run analysis jobs remained unexecuted. Water holdout and all oxygen samples are absent. No paired schedules or activations exist to audit. No truncated output was accepted.

Accepted water deposits sum to 0.056599893557760204 MeV; descriptive mean 0.00087076759319631124 MeV. The incomplete calibration (65/1024) cannot satisfy the fixed-design precision target; no successful-subset confidence claim is made. Both post-analysis runs exited 2 with complete=0 and propagation_supported=0. Conditional sampling intervals and calibration design-sensitivity intervals are both withheld. No complete-ensemble or biological conclusion follows.

Evidence root: .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/. Original 05-study evidence and driver logs are preserved; post-analysis operates on a separate full copy in 05-audit/post-analysis, preserving initial files. Both identity audits passed; all 65 completed process/module re-audits passed; every deposited-energy ledger balances to 0.5 MeV within 1e-8. All inventoried frozen and prior-input hashes match, 14594 stream seeds are unique and overlap with the entire 04-study assignment registry is zero. All 04 evidence remains excluded. See 05-audit/audit-summary.json, hash-verification.csv, energy-audit.csv, process-reaudit.csv and archive-manifest.csv. Archive enumeration excludes its own output. No remaining native worker was observed after termination.

READY for parent independent result/incident review only. Ticket04 acceptance remains with the parent. The original 04 incident and historical authorization narrative below/above remain preserved. This separate 05 software failure does not authorize a new run or clock.
Hash verification was performed after terminal exit and before the authorized documentation updates. The live new-study declaration was subsequently appended with the terminal report; its frozen 05-study snapshot and input-bytes remain unchanged. Do not interpret that post-terminal documentation change as run-time hash drift.

### Ledger repair verification (synthetic only)

> **Not scientific evidence.** The `05-ledger-*` directories used mock or
> synthetic exits to test the repaired runner; they are excluded from Study06's
> data. Study06 later ran the repaired runner natively.

The original Move-Item -Force failure is reproduced with readers lacking FileShare.Delete (05-ledger-fix-repro/Reproduce.ps1 and RedOutput.txt). This establishes a Windows sharing failure mechanism, not the identity of the original incident's handle; monitoring attribution remains unproven. Publication now serializes a unique same-directory candidate, flushes it, and uses File.Replace (File.Move for first creation). Failed candidates remain available. Only writer sharing/lock errors 32/33 retry, bounded by 500 ms and the remaining study budget; persistence failure before launch does not count as a native attempt. No native process is retried.

Repeated concurrent opens exposed transient pathname-unavailable/sharing errors during Windows replacement. Read-CalibratedLedger now opens with ReadWrite|Delete sharing and retries only open errors 2/32/33 for up to 500 ms. Successfully opened snapshots parsed completely; callers should use this reader rather than assume every raw open succeeds. Encoding remains PowerShell Export-Csv's default (UTF-8 without BOM in the tested pwsh environment), with existing field schema unchanged. These results do not establish universal filesystem, interruption, or power-loss durability.

The pre-existing lifecycle4 test completed without restart: 5380 reserved slots, exactly 5379 mock launches and one unexecuted_persistence_failure. The retained authoritative LifecycleOutput4.txt records its pass. Subsequent focused tests passed against the final helper: DriverFinal3.txt (actual production orchestration section, zero-arrival lifecycle, prelaunch lock, completed-launch terminal lock, launcher exception and finally status), FaultsFinal1.txt (transient lock and duplicate-launch prevention), and SnapshotsFinal6.txt (267 concurrently opened/parsed snapshots, accepted/failure/censor states, deadline and cap). The driver harness substitutes external launch/audit closures and synthetic setup only; no production skip-audit switch exists. It does not exercise runtime provenance setup or certify scientific results. Earlier failed snapshot-test outputs are retained.

Fresh full Debug TestCalibratedBeamOn and TestCalibratedPreflight checks passed (RunnerFinal1.txt and PreflightFinal1.txt); PowerShell parser and git diff --check passed. Evidence is under .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/. No scientific executable or study was launched for this verification. Original scientific evidence remains unchanged. Parent independent repair/result review remains required; this does not authorize a third study.

### Follow-up verification after independent review
The prelaunch argument-file write now precedes native counting. A failed write retains `unexecuted_argument_failure`, stops execution with a reason, and launches nothing; the focused negative fixture passes. Remaining time is recomputed after argument persistence.

The first full mocked driver run failed its expected totals because script-mock exits did not update the closure-local LASTEXITCODE as native exits do. That failed evidence remains in 05-ledger-full-driver-reviewed-fix-evidence. The corrected test uses an explicit mock return-value adapter and checks all four injected nonzero outcomes before the long run; production launch behavior was not changed for this test correction.

The current full mocked driver retained CALIBRATED_DRIVER_PASS and exit 0: 5380 slots, 5360 mock launches, 5356 accepted, two failed, two censored, and 20 unexecuted; 590 concurrent snapshots had no reader failures. Small driver and argument-write/ledger fault tests also passed. Evidence: 05-ledger-full-driver-mock-exit-fixed-evidence/handoff.md and its stdout, exit codes, and source hashes. Source hashes did not drift. The mock adapter excludes native exit semantics and startup provenance from full-run coverage; only the pure planner ran natively. Scientific runs04/05 remain incomplete and unchanged. Repair acceptance awaits fresh independent review; no new study is authorized.

## Authorized study06 terminal outcome — 2026-11-08 UTC

> **Status update (2026-11-09).** The line below that reads "READY for
> independent RESULT review" is the state at the end of the run; the review
> follows in "Independent result review and local acceptance". The accepted
> result, tables, interval definitions, limits and handoff are consolidated in
> [beam-on-fresh-source-calibration.md](beam-on-fresh-source-calibration.md) and
> [the handoff](beam-on-fresh-source-calibration-handoff.md). That page also
> records the post hoc source-count and cross-source-reaction checks that this
> section does not contain.

Study06 invoked the reviewed runner exactly once at HEAD 77f299b6c97d7fe975307e1100f9c148553947bb. Scientific start 2026-11-08T22:01:45.3126158Z; absolute deadline 2026-11-09T02:01:45.3126158Z. It exited 0 at 2026-11-08T23:33:19.9158065Z after 5494.5191028 seconds. All 2901 physical captures, 512 histories, 256 schedules and four analysis jobs were accepted. No failed/censored jobs, replacements, runner retries or clock reset; 1707 unused fresh slots remain unexecuted. One complete zero-deposit capture was retained. Both conditions completed 1024 calibration, 256 holdout and 128 fresh paired realizations.

Both fixed-design precision targets were missed: relative 95% mean interval half-width 8.05398% water and 8.01033% oxygen, versus 5%. Both complete paired ensembles pass the declared propagation support gates. Conditional fixed-intensity intervals and separate calibration-only/combined bootstrap sensitivity are retained for all metrics; combined intervals remain bounded exploratory approximations with unquantified tail contributions, not unconditional coverage or biological equivalence. No precision-qualified comparison is claimed and no additional sampling is authorized.

Evidence: .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/06-study and 06-audit/local-report.md. Actual scientific startup inventoried 19602 prior inputs including 58 registries; 14594 new seeds are unique and disjoint, with all six control seeds reserved. All inventoried frozen/prior hashes match. 7088 native re-audits passed (3673 process, 2901 capture, 512 history, two identities); all 2901 energy ledgers balance. Postanalysis on a separate copy reproduces both original summaries, support tables, derived streams and capture ledgers byte-for-byte. The initial copied analysis exited 2 with missing/censored/duplicate-pair diagnostics. After logged path corrections, copied analyses exited 0 and reproduced the original outputs. Path relocation is the supported explanation; exclusive causation was not established. This was not a scientific retry. Original study before/after hashes show zero changes. No native workers remained at terminal checks. Preservation applies to inventoried evidence, not every unrelated file or an atomic filesystem snapshot.

READY for independent RESULT review. No ticket acceptance, source edits, new controls, staging, commit, push, PR, GPS state transition or delegation. Earlier 04/05 outcomes remain unchanged; no later experiment authorized.


### Independent result review and local acceptance
Parent accepts ticket04 as completed bounded execution and truthful reporting, not achievement of the5% precision target. Two fresh independent reviews found no scientific acceptance blocker; the sole report finding (exclusive copied-analysis failure attribution) is qualified above. Independently checked: all prior input hashes and58 registries/reservations, derived-seed collision absence,512 histories/51874 observed activation identities and times,256paired packet/time identities, all archive CRCs and79397 manifest file hashes, module paths/private-core hash, and relevant configured11.4.3 source implementations. Earlier review confirmed accounting, energies/means/SE, support tables and39663 original-study preservation hashes.
Remaining limits: bootstrap streams were not independently regenerated from domain constants; full molecule payload and every schedule row were not independently reconstructed. Application timestamps are not authenticated external clocks; matching private modules/configured sources do not prove every binary build origin or wholesale physics validity. Both calibration targets missed; tail contributions remain unquantified. No additional sampling authorized. No push or PR.
The existing06-evidence.zip remains the immutable pre-review archive. This final report qualification is a post-review supplement; the corrected local report and tracked documentation supersede its causal wording, not its scientific data.
