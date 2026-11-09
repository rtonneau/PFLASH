# Fresh-source calibration implementation plan

> For agentic workers: use superpowers:executing-plans for inline execution, or superpowers:subagent-driven-development if delegated execution is selected. Follow the four gps tickets below and their verification gates.

**Goal:** Establish bounded, independently calibrated fresh-source beam-on evidence with explicit uncertainty and failure reporting.

**Architecture:** Pure C++ capture/statistics/audit helpers feed a capped process runner. Disjoint calibration, holdout and fresh identities preserve the approved sampling design; paired histories support fixed-calibration inference and separately labeled calibration sensitivity.

**Tech Stack:** C++20, Geant4 11.4.3 private runtime, MSVC/Ninja, CTest and existing PowerShell/Job Object orchestration.

**Spec:** .claude/plans/beam-on-fresh-source-calibration.md (approved).

## Global Constraints

All approved design limits apply unchanged. See Assumptions and the frozen protocol ticket below for paths, process budgets and sampling counts. No execution is authorized by this planning artifact alone.

## Review Focus

- Files left by failed captures must not enter calibration (ticket 1).
- Seed or split collisions must fail before sampling (tickets 1 and 3).
- Missing rate arms must not be interpreted as zero counts (tickets 2 and 3).
- Weight diagnostics cannot establish support for the unobserved count tail (ticket 2).
- Deadline exhaustion must leave explicit unexecuted identities (ticket 3).

**Branch:** test/beam-on-fresh-source-calibration
**Estimated effort:** 1–2 days implementation and review; study execution capped at four hours

## Strategy

Implement the approved design in .claude/plans/beam-on-fresh-source-calibration.md, starting from ffe1a57. Keep numerical logic in C++20 pure helpers and use existing PowerShell runners only for process orchestration. Establish capture acceptance before statistics, then integrate the runner and execute only after targeted verification. Preserve the approved design as the specification; this plan introduces no new study arms or sampling allowances.

## Tickets Overview

- **Ticket 1:** Validate capture evidence and allocate disjoint sample identities and random streams.
- **Ticket 2:** Implement deposited-energy statistics and paired Poisson uncertainty propagation.
- **Ticket 3:** Freeze and orchestrate the bounded calibration, holdout and fresh ensemble protocol.
- **Ticket 4:** Run, independently audit, document and locally commit the bounded study.

## Sequencing Rationale

Statistics consume only accepted capture records. The runner consumes tested statistics and identity allocation. Execution and scientific claims require all three implementations and the frozen protocol. Each implementation ticket includes regression checks and a local commit; no push or PR.

## Risks & Mitigation

- **Risk:** Packet files exist after a later capture failure. → **Mitigation:** Require process completion, validation marker, parsed packet, energy balance and provenance together.
- **Risk:** Seed collisions or outcome-dependent replacement. → **Mitigation:** Preassigned identity/stream ledger, disjoint ranges, collision audit and immutable attempt statuses.
- **Risk:** Bootstrap weights hide absent high-count support. → **Mitigation:** Report Poisson tail, ESS and weight concentration; enforce the approved support gates and bounded interpretation.
- **Risk:** Missing arms masquerade as zero populations. → **Mitigation:** Require complete pairs before zero-filling genuinely absent species.
- **Risk:** Last process exceeds the study budget. → **Mitigation:** Monotonic elapsed time, remaining-time launcher deadlines and explicit unexecuted records.

## Assumptions

- The approved specification governs all numbers, estimands, failure policies and support gates. Its approved calibration budget is 1024 + 256 attempts per condition; fresh comparisons use 128 paired realizations per condition at 100/1000 Gy/s.
- Resolve paths through .claude/.claude-project.json and ~/.claude/CONFIG_RESOLUTION.md. Follow .claude/geant4-instructions.md for builds and Debug tests. Read relevant configured Geant4 .cc implementations and established examples before changing simulation hooks.
- Reuse the private runtime only after before/after hashes, loaded modules and retained patch provenance pass. Do not modify shared source/install or old evidence.
- New evidence root is .scratch/tests/2026-11-08__beam-on-fresh-source-calibration. Define $probe as its private probe build and $evidence as its new study output directory in the execution transcript. Never overwrite existing output.
- Four-hour ceiling applies to study execution, including capture, schedule and history jobs; builds and preflight controls are separately bounded and recorded. Attempt/history maxima are ceilings, not quotas to fill after failures.

--- ticket: 01-capture-evidence ---

**Acceptance Criteria:**
- [ ] Preassign condition, split, realization, arrival and stream identities; collision-check seed allocation against this study and prior retained evidence. Calibration, holdout and fresh captures are disjoint.
- [ ] Accept complete zero-energy/zero-molecule packets. Reject truncated, aborted, unbalanced, incomplete, nonfinite or provenance-invalid captures even if a packet exists.
- [ ] Preserve intended, attempted, accepted, rejected, censored and unexecuted identities; never replace rejected records or compute accepted-subset precision as complete-sample precision.
- [ ] Complete targeted Debug tests and local commit.

**Files to Touch:**
- `test/timed-injection/probe/CaptureEvidence.hh` (new pure record/validation interface)
- `test/timed-injection/probe/CaptureEvidence.cc` (new)
- `test/timed-injection/probe/CaptureEvidenceTest.cc` (new)
- `test/timed-injection/probe/TrackCapture.cc` (only validation gaps demonstrated by tests)
- `test/timed-injection/probe/CMakeLists.txt`

**Verification Step:**

Run:
```powershell
ctest --test-dir $probe -R '^(CaptureEvidence|PacketAndSource)$' --output-on-failure --no-tests=error
```

Expected:
Both Debug tests pass, including valid empty events and invalid files left by failed processes.

**Notes:**
Define namespace Calibration with enum class Split { Calibration, Holdout, Fresh }; struct CaptureRecord containing condition, split, realization, arrival, unique capture ID, three capture seeds, status, energyMeV, escapedMeV, massKg, halfSide and compatibility fingerprint. Define ValidateCapture(const CaptureRecord&, const Packets::Packet&) to throw on inconsistent acceptance evidence, and ValidateIdentities(const std::vector<CaptureRecord>&) to throw on duplicate identities/seeds or split overlap. Store process/marker/provenance flags explicitly in CaptureRecord. IDs establish independence; identical content hashes alone do not prove reuse (valid zero captures may coincide).

- [ ] Add failing fixtures for zero deposit, truncated packet with success-looking file, missing marker, energy mismatch, duplicate ID/seed, wrong geometry/fingerprint and nonfinite energy.
- [ ] Implement validators and explicit serialization schema for the attempt ledger; reuse Packets::Read/Validate.
- [ ] Run the command above, then commit only this ticket's files.

--- ticket: 02-calibration-statistics ---

**Acceptance Criteria:**
- [ ] Fixed-size mean, SD, SE, Student-t 95% interval, relative half-width, zero fraction and quantiles include complete zeros; nonpositive means and incomplete splits cannot qualify calibration.
- [ ] Holdout is reported separately with calibration-minus-holdout uncertainty; holdout cannot enter the calibrated mean or bootstrap calibration pool.
- [ ] Implement 2000 seeded independent calibration/paired-realization bootstrap draws, unnormalized Poisson likelihood weighting once per pair, conditional-only intervals and separately labeled combined design sensitivity.
- [ ] Enforce ESS >=64, largest normalized weight <=0.10 and P(N>8)<=0.001 for every replicate; report tails and probability of any overflow in 128 histories. No clipping, renormalization to capped counts or dropped replicates.
- [ ] Missing pairs block inference; species union handles absent species in otherwise complete arms. Numerical checks pass and local commit is complete.

**Files to Touch:**
- `test/timed-injection/probe/CalibrationStatistics.hh` (new)
- `test/timed-injection/probe/CalibrationStatistics.cc` (new)
- `test/timed-injection/probe/CalibrationStatisticsTest.cc` (new)
- `test/timed-injection/probe/AnalyzeCalibratedBeamOn.cc` (new CLI)
- `test/timed-injection/probe/CMakeLists.txt`

**Verification Step:**

Run:
```powershell
ctest --test-dir $probe -R '^(CalibrationStatistics|FiniteScheduleTest)$' --output-on-failure --no-tests=error
```

Expected:
Both tests pass; analytic compound-Poisson expectations and deterministic seeded Monte Carlo agree within predeclared test tolerances.

**Notes:**
Interfaces: Calibration::EnergySummary SummarizeEnergy(const std::vector<double>& energyMeV); double PoissonWeight(unsigned count, double nominalMean, double variedMean); double PoissonOverflow(double mean, unsigned maximum); ComparisonSummary AnalyzeComparison(const std::vector<double>& calibrationMeV, const std::vector<PairedOutcome>& outcomes, std::uint64_t bootstrapSeed). EnergySummary exposes count, mean, sampleSD, standardError, lower95, upper95, relativeHalfWidth, zeroFraction and quantiles. PairedOutcome stores complete status, count and per-metric paired arm values. ComparisonSummary records intervals, support diagnostics and refusal reasons. CLI consumes the validated ledger and history files, writes a versioned machine-readable summary and a readable report.

- [ ] Write deterministic tests for [0,2] mean=1 and SE=1, all-zero refusal, incomplete sample refusal, holdout exclusion and identity weight when both Poisson means agree.
- [ ] Test exact Poisson normalization/first two moments using a sufficiently long count sum; paired weight is applied once, not squared. Add seeded schedule Monte Carlo with declared tolerances and stream-invariance checks.
- [ ] Test support-threshold equality/failure, underflow/overflow, missing arm, species present in only one arm and independent bootstrap streams.
- [ ] Implement numerical methods with verified Student-t quantiles, stable log weights/tail evaluation and recorded resampling algorithm/seeds. Run tests and commit.

--- ticket: 03-bounded-study-runner ---

**Acceptance Criteria:**
- [ ] Freeze tracked protocol and exact seed/attempt manifest before any physical study sampling. Preserve protocol bytes and hashes with results.
- [ ] Run 1024 calibration + 256 holdout attempts per condition with frozen calibration before holdout inspection. Target miss is explicit and cannot enlarge the budget.
- [ ] Generate one normalized Poisson schedule per fresh realization, scale to both fixed exposure durations, capture each arrival independently once and reuse only across the paired rate arms.
- [ ] Preserve all empty/censored/failed outcomes, strict eight-source cap and no replacements. Maximum 4608 physical attempts, 512 chemistry histories and 256 schedule jobs; four-hour ceiling includes remaining-time process caps.
- [ ] Preserve 512 MiB per-process cap, 60 s maximum deadline, 3 GiB free-memory floor, independent streams, per-process provenance and immutable old evidence.
- [ ] Synthetic orchestration checks pass and local commit is complete.

**Files to Touch:**
- `test/timed-injection/probe/CalibratedBeamOnProtocol.md` (new)
- `test/timed-injection/probe/RunCalibratedBeamOn.ps1` (new, follows existing capped runner)
- `test/timed-injection/probe/TestCalibratedBeamOn.ps1` (new synthetic runner/audit fixtures)
- `test/timed-injection/probe/FreshSchedule.cc`
- `test/timed-injection/probe/CheckCalibratedBeamOn.cc` (new ledger/paired-output audit CLI)
- `test/timed-injection/probe/CMakeLists.txt`

**Verification Step:**

Run:
```powershell
./test/timed-injection/probe/TestCalibratedBeamOn.ps1 -BuildDir $probe -OutDir "$evidence-runner-checks"
```

Expected:
CALIBRATED_BEAM_ON_CHECKS_PASS; intentional invalid fixtures rejected, no physical study launches.

**Notes:**
Runner parameters: BuildDir, FinderBuildDir, InstallPrefix, OutDir. Resolve and hash exact inputs; refuse an existing OutDir. C++ helpers own statistics/validation; PowerShell owns process launch and ledger persistence. Keep old RunFreshBeamOn.ps1 and its prior evidence reproducible. One schedule retains normalized arrival positions; scale to T=.002/rate and observe T+2 ps. Empty histories need compatible geometry metadata but import no source packet.

- [ ] Add synthetic launcher fixtures for deadline expiry, memory cap, early failure and ninth arrival; assert no resampling or successful-subset inference.
- [ ] Test no within-realization packet reuse, calibration/holdout/fresh overlap rejection, source/chemistry seed independence and paired time scaling/count/energy identity.
- [ ] Add malformed/missing activation and energy-ledger tests to CheckCalibratedBeamOn; require complete pairs and exact planned capture IDs.
- [ ] Freeze protocol including all numerical gates and seed ranges; run synthetic checks plus existing analysis regression, then commit.

--- ticket: 04-execute-audit-report ---

**Acceptance Criteria:**
- [ ] Verify runtime provenance and focused native controls before starting the immutable study clock and attempt ledger.
- [ ] Execute once under the approved budget; report completed, failed, censored and unexecuted totals without replacing failures or extending precision/budget.
- [ ] Independently audit captures, activations, energy sums, pair identities, seed separation and analyzer output; review code and scientific interpretation.
- [ ] Report target met/missed, holdout comparison, conditional intervals, combined design sensitivity or explicit unsupported status, resource use and precise reproducibility paths.
- [ ] Commit validated local changes and documentation; no PR, push, shared runtime mutation or full-dose extrapolation.

**Files to Touch:**
- `docs/dose-rate/beam-on-irt.md`
- `test/timed-injection/probe/CalibratedBeamOnProtocol.md` (interpretation/reproduction only; preserve frozen run bytes)
- `.claude/plans/beam-on-fresh-source-calibration.md` (execution status)
- `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/` (new ignored evidence)

**Verification Step:**

Run:
```powershell
& "$probe/CheckCalibratedBeamOn.exe" $evidence
& "$probe/AnalyzeCalibratedBeamOn.exe" $evidence
```

Expected:
Audit validates the ledger, including honest incomplete/censored statuses. Analyzer emits qualified results only for eligible complete conditions; otherwise explicit incomplete/unsupported outcomes. Scientific success is not required to pass truthful reporting.

**Notes:**
- [ ] Read source/example implementations, verify private runtime hashes and loaded DLL provenance, and run the existing relevant native controls and new pure Debug tests. Record exact commands, configuration and executable hashes.
- [ ] Start RunCalibratedBeamOn with a fresh evidence directory and the agreed fixed protocol; preserve every job log/exit/status and before/after hashes. No cap changes or retry-as-success.
- [ ] Run independent ledger audit and analysis, then fresh read-only code/scientific review. Correct software defects with targeted tests; never silently reuse invalidated scientific evidence or expand this run's budget. Any required new study needs a separately declared run.
- [ ] Update documentation with supported conclusions and limitations; review diff, commit local validated work and retain ignored artifacts with an archive manifest.


## Authorized study06 terminal outcome — 2026-11-08 UTC

Study06 invoked the reviewed runner exactly once at HEAD 77f299b6c97d7fe975307e1100f9c148553947bb. Scientific start 2026-11-08T22:01:45.3126158Z; absolute deadline 2026-11-09T02:01:45.3126158Z. It exited 0 at 2026-11-08T23:33:19.9158065Z after 5494.5191028 seconds. All 2901 physical captures, 512 histories, 256 schedules and four analysis jobs were accepted. No failed/censored jobs, replacements, runner retries or clock reset; 1707 unused fresh slots remain unexecuted. One complete zero-deposit capture was retained. Both conditions completed 1024 calibration, 256 holdout and 128 fresh paired realizations.

Both fixed-design precision targets were missed: relative 95% mean interval half-width 8.05398% water and 8.01033% oxygen, versus 5%. Both complete paired ensembles pass the declared propagation support gates. Conditional fixed-intensity intervals and separate calibration-only/combined bootstrap sensitivity are retained for all metrics; combined intervals remain bounded exploratory approximations with unquantified tail contributions, not unconditional coverage or biological equivalence. No precision-qualified comparison is claimed and no additional sampling is authorized.

Evidence: .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/06-study and 06-audit/local-report.md. Actual scientific startup inventoried 19602 prior inputs including 58 registries; 14594 new seeds are unique and disjoint, with all six control seeds reserved. All inventoried frozen/prior hashes match. 7088 native re-audits passed (3673 process, 2901 capture, 512 history, two identities); all 2901 energy ledgers balance. Postanalysis on a separate copy reproduces both original summaries, support tables, derived streams and capture ledgers byte-for-byte. The initial copied analysis exited 2 with missing/censored/duplicate-pair diagnostics. After logged path corrections, copied analyses exited 0 and reproduced the original outputs. Path relocation is the supported explanation; exclusive causation was not established. This was not a scientific retry. Original study before/after hashes show zero changes. No native workers remained at terminal checks. Preservation applies to inventoried evidence, not every unrelated file or an atomic filesystem snapshot.

READY for independent RESULT review. No ticket acceptance, source edits, new controls, staging, commit, push, PR, GPS state transition or delegation. Earlier 04/05 outcomes remain unchanged; no later experiment authorized.

