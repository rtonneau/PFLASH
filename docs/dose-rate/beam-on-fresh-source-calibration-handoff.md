# Handoff: after the fresh-source calibration study (Study06)

Start here in the next session. The result, its limits and its tables are in
[beam-on-fresh-source-calibration.md](beam-on-fresh-source-calibration.md); this
page only says where things are and what is open. Design and incident history:
[beam-on-irt.md](beam-on-irt.md) and `.claude/plans/beam-on-fresh-source-calibration*.md`.

## State in one paragraph

Study06 ran once, completed inside its 4 h budget, and is accepted as a truthful,
audited execution. Both 5% precision targets were missed (8.05% water, 8.01%
oxygen), so every comparison is exploratory, and the 1000 minus 100 Gy/s
differences are not attributed to inter-track chemistry. Nothing is authorized
beyond what ran: no more sampling, no new study, no push, no PR. All four tickets
are accepted locally. The GPS session
`.work/sessions/2026-11-08__beam-on-fresh-source-calibration/` is `finish-pending`
because `/gps finish` publishes; do not run it while publication is prohibited.

## Commits (branch `test/beam-on-fresh-source-calibration`, nothing pushed)

| Commit | Meaning |
|---|---|
| `ffe1a57` | Baseline |
| `2bd4919`, `4c8b2c0`, `2c3c7ed` | Tickets 01 (capture evidence), 02 (calibration statistics), 03 (bounded runner bound to frozen provenance) |
| `5e7df33` | Evidence protection after the Study04 incident |
| `e57d42d`, `6c7df6f`, `bd612d5` | Study05 authorization; frozen-stream verification; ledger repair after the Study05 failure |
| `27ff9b7`, `7b6b326` | Study06 proposal; Study06 authorization |
| **`77f299b`** | **Code Study06 ran at** (reserves all prior seed registries). `git diff 77f299b..HEAD` touches only four doc or plan files, so the runner is unchanged since |
| `dcae267`, `7284804` | Study06 results report and independent review; whitespace |

The commit that adds this page is the one after `7284804`; find it with
`git log -- docs/dose-rate/beam-on-fresh-source-calibration-handoff.md`.

## Runtime and patch provenance

All paths below are relative to the repository root; `.scratch/` and `.work/` are
git-ignored and exist only on this machine.

| Item | Location and identity |
|---|---|
| Private Geant4 11.4.3 runtime | `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install` (the shared Geant4 install is not modified). `install/bin/G4processes_core.dll` SHA-256 `11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B`, hardcoded at line 16 of `RunCalibratedBeamOn.ps1` |
| Patches (`patches/geant4/11.4.3/`) | `molecular-finder/coincident-neighbors.patch` `FB6FAA46…04BE`; `timed-injection/source-contacts-experimental.patch` `F3FA0E11…9FE1`; `timed-injection/meso-boundaries-experimental.patch` `A3DB3958…94C1`. Full hashes: `Get-FileHash` on those files. They were last changed before the runtime was built (`186ccf5`, `5e60636`, `a695d57`) |
| Not in this runtime | `meso-bulk-instrumentation-experimental.patch` |
| Source comparison | `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/04-preflight/source-check.txt` |
| Runtime build records | `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/`: `Build.cmd`, `BuildProbesV3.cmd`, `kernel-build-v4-contact.meta`, `probes-build-v3.meta` |
| Probe executables | `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/03-probe-release/` (`TrackCapture4um`, `FreshSchedule`, `BeamOnHistory`, `CheckCalibratedBeamOn`, `AnalyzeCalibratedBeamOn`); hashes in `06-preflight/binary-verification.csv` |
| Job launcher | `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/finder-probe/JobLauncher.exe` |
| Frozen inputs at run time | `06-study/input-bytes/`, `input-snapshots.csv`, `protocol-at-run.md`, `new-study-declaration.md` |

Matching module paths, the core DLL hash and the configured sources do not prove
the origin of every binary or the correctness of the physics (report section 7).
Treat `test/timed-injection/probe/*` and the protocols as frozen study inputs: a
new design adds new files or a new protocol version, and a new study snapshots
them again.

## Where the evidence is

Root: `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/`.

| Need | Path (under the root) |
|---|---|
| Per-condition data | `06-study/water/`, `06-study/oxygen/` |
| Frozen calibration | `<cond>/frozen-calibration.tsv`, `frozen-calibration-hash.csv`, `calibration.summary.tsv`, `calibration.report.txt` |
| Fresh packets | `<cond>/<cond>-capture<N>.{packet,energy,chemistry,meta,args,…}`; `captures.tsv` maps capture id to split, realization, arrival, streams (split 0 calibration, 1 holdout, 2 fresh) |
| Schedules | `<cond>/pair<R>-schedule.*`, `pair<R>-100.schedule`, `pair<R>-1000.schedule`, `pair<R>-100.schedule.normalized`; seeds in `pairs.tsv` |
| Histories | `<cond>/pair<R>-100.history`, `pair<R>-1000.history` (+ `.calibration`, `.meta`); `pair<R>.manifest` lists packets (an empty realization holds one placeholder line naming a calibration packet, not a source) |
| Ledgers | `<cond>/planned-ledger.txt`, `capture-ledger.txt`, `capture-index.txt`, `paired-index.txt`, `analysis-input.txt`; study-level `06-study/processes.csv`, `streams.tsv`, `prior-seeds.txt` |
| Analysis outputs | `<cond>/analysis.{summary.tsv,support.tsv,report.txt,derived-streams.tsv}` |
| Reviewed report | `06-audit/local-report.md` (post-review supplement), `06-audit/handoff.md` (old wording on one point, superseded) |
| Audit records | `06-audit/audit-summary.json`, `native-audits.csv`, `energy-audit.csv`, `hash-verification.csv`, `study-preservation-{before,after}.csv`, `postanalysis-reproduction.csv`, `post-review-report-hash.csv` |
| Reproduction record of the copied re-analysis | `06-audit/post-analysis/`, `copy-path-relocations.csv`, `first-postanalysis-*` |
| Archive and integrity | `06-evidence.zip`, `06-evidence.zip.sha256.txt`, `06-archive-scope.txt`, `06-archive-verification.txt`, `06-audit/archive-manifest.csv` |
| Blocked first preflight (not data) | `06-preflight/handoff.md` |
| Reused native controls (seeds `900000001`–`900000006`) | `05-preflight/control-*`, re-hashed in `06-preflight/control-verification.csv` |
| This handoff's checks | `07-handoff-verification/` (README lists each script and output) |

## Integrity and reproduction (none of this launches sampling)

Environment for the native checkers (from `06-audit/audit.ps1`):

```powershell
$install = (Resolve-Path '.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install').Path
$env:PATH = "$install/bin;$env:DEV_DIR/vcpkg/installed/x64-windows/bin;$env:PATH"
```

1. **Archive.** Recompute `Get-FileHash 06-evidence.zip -Algorithm SHA256`; it must
   be `B119CE975F8991EB37158F23D4B4622415ECBDBB7541355BE8A21B1BE580214F`.
   `python -I 07-handoff-verification/verify_archive.py <evidence-root>` (read-only,
   about 9 minutes) re-checks every zip member against `archive-manifest.csv`.
   Expected: 79397 of 79397 manifest rows match in the zip; 79396 match the live
   files, the exception being `06-audit/local-report.md`.
2. **Report supplement.** The zip holds the pre-review report
   (`4BA1026A…78F4`, 13729 bytes, copy in `07-handoff-verification/`); the live file
   is `70A59F58…2F26` (15409 bytes, `post-review-report-hash.csv`). They differ only
   in the final paragraph and the appended review section. Later supplements go in
   new, separately hashed files; do not edit the zip or `06-*`.
3. **Re-audit and re-analysis.** Run only on a **copy** of `06-study`. `audit.ps1`
   hardcodes `06-audit` as its output directory and `AnalyzeCalibratedBeamOn.exe
   <condDir> [--calibration-only]` writes `analysis.*` into the directory it is
   given, so running either on the originals would overwrite recorded evidence.
   The copy needs its path-bearing `.args`, `.history` and `.manifest` files
   relocated (the script does this and logs it); skipping that is what made the
   first copied analysis exit 2. Checker modes: `CheckCalibratedBeamOn.exe plan root
   prior-seeds | capture conditionDir id install | identities conditionDir | history
   stem rate install | process stem marker install`.
4. **Re-running the study** is a new sampling decision, not reproduction.
   `RunCalibratedBeamOn.ps1 -BuildDir …/03-probe-release -FinderBuildDir
   …/beam-off-meso/finder-probe -InstallPrefix …/beam-off-meso/install -OutDir
   <new dir>` refuses an existing `-OutDir`, has no resume mode, needs its own
   authorization, clock and seed registries, and must treat all 19602 prior inputs
   (58 registries) plus the Study06 14594 seeds and the six control seeds as used.

## Inputs reusable for matched independent-track controls

The question is whether the 1000-minus-100 Gy/s differences survive when the same
sources are run one at a time, so that molecular age and delivery spacing are
fixed and only the other tracks are removed.

Reusable as inputs:

- The 341 fresh packets (158 water, 183 oxygen) with their schedules and
  normalized arrival times, via `captures.tsv` and `pair<R>-*.schedule*`.
- The frozen calibration means (7.2052e-4 and 6.8130e-4 MeV), the fixtures
  `MesoFixture4um.in` and `MesoFixture4umOxygen.in`, `BeamOnHistory.exe` and the
  private runtime.
- The six native controls (`900000001`–`900000006`) as already-hashed references.

Not reusable, or not free:

- Study04 and Study05 captures: excluded, not accepted data.
- The 1707 unexecuted fresh slots hold seeds without packets; using them is new
  sampling.
- A single-source control needs **new, independent chemistry seeds** and a new
  authorization; the existing paired histories cannot be split afterwards.
- `IndependentHistories.cc` and `RunIndependentHistories.cmake` are earlier
  probes, unverified for this purpose.
- In oxygenated water the O2 pool is shared inside one history; independent
  single-source histories will not reproduce that coupling, which is the point of
  the comparison but also means `oxygen_pool_change` is not additive.

## Open scientific questions

1. **Attribution.** How much of the 1000-minus-100 Gy/s contrast is molecular age
   at observation (2 µs against 20 µs), arrival spacing, or inter-track
   reactions? The `disjoint` scan (2 events in 512 histories, both at 100 Gy/s)
   argues against direct cross-source reactions but cannot see ancestry-free
   partners, downstream products or bulk-pool coupling.
2. **Oxygen arrival-count excess.** 183 observed against 150.1 expected (exact
   p = 0.0089, not corrected for the two conditions examined). The draws use the
   frozen mean, so this is not calibration error; whether it is chance or a
   schedule-generator defect has not been checked. Do this first, since it is
   read-only.
3. **Precision target.** 8% against 5%. At the same spread, roughly 2650
   calibration captures per condition would be needed (arithmetic, not a plan);
   whether that is worth spending is a user decision.
4. **Unquantified tail.** Histories with more than 8 arrivals were never
   simulated; the gates (P(N > 8) at most 1.1e-5) bound it only operationally.
5. **Residual review limits.** Bootstrap streams were not independently
   regenerated from their domain constants; the full molecule payload and every
   schedule row were not independently reconstructed.
6. **Geometry and dose.** Nothing here speaks for 0.5 mm geometry, doses up to
   10 Gy, a mesoscopic tail or production streaming.

## Resource limits observed

| Resource | Observed | Cap or note |
|---|---|---|
| Study wall time | 5494.5 s | 4 h absolute deadline per study |
| Job time | captures 4535 s (about 1.56 s each), histories 278.4 s (max 19.5 s), schedules 53.8 s, analysis 29.8 s | Capture cost dominates; calibration precision scales with capture count |
| Per-job memory | at most 57.7 MiB | 512 MiB |
| Per-job time | at most 19.34 s (launcher-reported; 19.50 s by ledger start and finish) | 60 s |
| Free memory | at least 5.44 GiB | floor 3 GiB |
| Disk | `06-study` 214 MB, `06-audit` 257 MB, zip 112,282,940 bytes | |
| Job count | 3673 jobs, 0 failures, 0 cap notifications | The archive manifest lists 79397 files |

## Constraints that still hold

No push, PR, additional sampling, clock reset, edit of the immutable archive,
or edit of frozen probe inputs without explicit new authorization. Do not delete
scratch directories, failed-run evidence, original checkpoints, private builds or
ignored GPS state. Documentation edits made after the run do not alter the frozen
snapshots in `06-study/input-bytes/`, `protocol-at-run.md` or
`new-study-declaration.md`.
