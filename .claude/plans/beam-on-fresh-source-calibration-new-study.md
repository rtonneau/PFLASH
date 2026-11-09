# Authorized fresh calibration study

> **Status (2026-11-09): closed.** This file holds, in order, the Study05
> authorization, the Study05 failure, the Study06 proposal, its authorization and
> its outcome. Study06 is the accepted study; Study05 is excluded. Accepted
> result and limits:
> [docs/dose-rate/beam-on-fresh-source-calibration.md](../../docs/dose-rate/beam-on-fresh-source-calibration.md);
> next session:
> [handoff](../../docs/dose-rate/beam-on-fresh-source-calibration-handoff.md).
> Status headings below that still read "proposed" or "awaiting" are superseded.

The interrupted 04-study and rewritten-control incident remain preserved and excluded from this new study. This is a separately declared experiment, not a resumed clock or replacement of individual failed attempts.

Use the approved scientific design in beam-on-fresh-source-calibration.md and CalibratedBeamOnProtocol.md unchanged: 1024 calibration plus 256 holdout attempts per condition, 128 independent fresh realizations per condition paired at 100/1000 Gy/s, 5% relative 95% calibration interval half-width target. Complete zero deposits remain included; truncated/failed events remain recorded without replacement.

New additional budget: at most 4608 physical capture attempts, 512 chemistry histories and 256 schedule jobs, within one new four-hour wall-clock window beginning at the recorded scientific launch. Per-process limits remain 512 MiB, at most60 seconds and3GiB free memory. No extensions if target or completion is missed. All13 physical outputs from the interrupted run remain separate; none enter the new calibration/holdout/fresh sets.

Use new directories under .scratch/tests/2026-11-08__beam-on-fresh-source-calibration: 05-preflight for controls and05-study for scientific evidence. Reject either directory if it already exists; do not silently choose a replacement. Preflight uses New-CalibratedPreflightPlan and explicit resolved paths, with all29 destinations checked before creation and checked again immediately before launch under exclusive ownership. Record fresh control output hashes before further work. Never modify prior serialized arguments or write under03-native-controls or04-study.

The runner now inventories earlier same-session streams.tsv registries as prior assignments. Reserve all04-study assigned seeds (including unexecuted slots), all prior inventories and new control seeds before freezing the new manifest. A synthetic planner check allocated14594 new streams against14594 old assignments with zero overlap; this is validation evidence only, not the final scientific manifest. Verify final manifest disjointness before the first capture.

Freeze declaration, protocol, source/binary/runtime hashes, complete seed manifest and expected outputs before scientific launch. Reuse the reviewed private runtime only after provenance checks. Use explicit bounded commands and preserve every result. After completion/deadline, audit and report calibration/holdout and paired uncertainty where supported; no complete-ensemble inference from survivors. Obtain independent result review before local delivery. No push or PR.

User explicitly authorized this separately declared fresh study and its additional fixed budget on 2026-11-08, with delegated execution and local reporting. No push or PR; all original evidence remains preserved.


## Authorized 05-study terminal outcome — 2026-11-08

Launched exactly once at 2026-11-08T20:09:40.6845574Z with the approved new budget. Runner exited 1 after 109.2153426 seconds: RunCalibratedBeamOn.ps1 line 95, Move-Item replacing processes.csv reported "Cannot create a file when that file already exists." No retry, replacement, source modification or further sampling followed. The monitoring reader accessed processes.csv during execution; whether file sharing caused the replacement failure is unresolved. The finally block persisted status and after-hashes. Its empty stop_reason is not a success indication: the uncaught exception is the terminal cause.

65 accepted water calibration captures completed, including one complete zero deposit. The runner counted 66 attempts: slot water-capture65 was marked attempted before Save failed, has no native log/meta/energy output, and its capture audit rejected missing evidence. Thus 65 actual native captures, one rejected prelaunch slot, 4542 registry-unexecuted capture slots; zero censored native processes. All 512 histories, 256 schedules and four in-run analysis jobs remained unexecuted. Water holdout and all oxygen samples are absent. No paired schedules or activations exist to audit. No truncated output was accepted.

Accepted water deposits sum to 0.056599893557760204 MeV; descriptive mean 0.00087076759319631124 MeV. The incomplete calibration (65/1024) cannot satisfy the fixed-design precision target; no successful-subset confidence claim is made. Both post-analysis runs exited 2 with complete=0 and propagation_supported=0. Conditional sampling intervals and calibration design-sensitivity intervals are both withheld. No complete-ensemble or biological conclusion follows.

Evidence root: .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/. Original 05-study evidence and driver logs are preserved; post-analysis operates on a separate full copy in 05-audit/post-analysis, preserving initial files. Both identity audits passed; all 65 completed process/module re-audits passed; every deposited-energy ledger balances to 0.5 MeV within 1e-8. All inventoried frozen and prior-input hashes match, 14594 stream seeds are unique and overlap with the entire 04-study assignment registry is zero. All 04 evidence remains excluded. See 05-audit/audit-summary.json, hash-verification.csv, energy-audit.csv, process-reaudit.csv and archive-manifest.csv. Archive enumeration excludes its own output. No remaining native worker was observed after termination.

READY for parent independent result/incident review only. Ticket04 acceptance remains with the parent. The original 04 incident and historical authorization narrative below/above remain preserved. This separate 05 software failure does not authorize a new run or clock.

## Proposed study06 — awaiting separate user authorization
> **Superseded (2026-11-09).** The user authorized this proposal (see "Study06
> authorization" below), and it ran once to completion. The heading records the
> state when it was written. Outcome:
> [docs/dose-rate/beam-on-fresh-source-calibration.md](../../docs/dose-rate/beam-on-fresh-source-calibration.md).

Study05 is closed and excluded. Repair commit bd612d5 has passed the full mocked driver and fresh independent review. This proposal does not authorize execution until the user explicitly approves it.

Declare one new experiment in the fresh directory .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/06-study, with read-only preflight evidence in06-preflight and final audit in06-audit. Reject existing destinations; never replace them or silently choose another experiment. Preserve all04/05 captures, ledgers and failures unchanged; none enters06 calibration, holdout or fresh realization samples. Reserve all earlier assigned streams including unexecuted slots, synthetic registries and control seeds, then freeze and verify a disjoint06 registry before any scientific launch.

Additional fixed maxima:4608 physical capture attempts,512 chemistry histories and256 schedule jobs within one new four-hour absolute window starting at recorded scientific launch. Calibration1024 and holdout256 attempts per oxygen condition;128 fresh realizations per condition paired at100/1000Gy/s. Keep the predeclared5% relative95% interval-half-width target, assessed only after the fixed budget. Zero deposits count; failed/truncated attempts remain recorded without replacement. No precision-driven extensions, resuming, replaying attempts or resetting a clock after failure.

Use the existing accepted05 native controls after rechecking their retained hashes and native audits; this proposal adds no control simulations. Preflight revalidates the reviewed runner, binaries/private runtime, declaration and protocol, existing control evidence, exact new destinations and final stream disjointness. Any preflight failure stops before scientific launch; no bypass. Freeze current declaration (including subsequent explicit approval record), source/runtime hashes and manifests. No unreviewed source edits during execution. Use the production snapshot reader for monitoring.

Scientific settings, fixed-duration Poisson sampling, independent random streams, one-use fresh packets per realization, conditional intervals and separate bootstrap Poisson-reweighted calibration sensitivity, support/censor limits, per-process512MiB/60s and3GiB free-memory floor remain unchanged. Persist terminal outcomes, audit all evidence, withhold unsupported conclusions, and obtain independent result review. Mock verification does not certify native scientific evidence. No push, PR or shared Geant4 modification.

### Study06 authorization
The user explicitly authorized study06 and delegated execution/local reporting under the proposal above. This grants one additional fixed budget of at most4608 capture attempts,512 chemistry histories and256 schedule jobs in one new four-hour window. It does not authorize another experiment after failure, replacement attempts, extensions, push or PR. All04/05 evidence remains excluded and preserved.

## Authorized study06 terminal outcome — 2026-11-08 UTC
> **Update (2026-11-09).** "READY for independent RESULT review" at the end of
> this section is the state at the end of the run. Two independent reviews found
> no scientific acceptance blocker and the result was accepted locally as a
> completed bounded execution, not as meeting the 5% target. See
> [docs/dose-rate/beam-on-fresh-source-calibration.md](../../docs/dose-rate/beam-on-fresh-source-calibration.md).

Study06 invoked the reviewed runner exactly once at HEAD 77f299b6c97d7fe975307e1100f9c148553947bb. Scientific start 2026-11-08T22:01:45.3126158Z; absolute deadline 2026-11-09T02:01:45.3126158Z. It exited 0 at 2026-11-08T23:33:19.9158065Z after 5494.5191028 seconds. All 2901 physical captures, 512 histories, 256 schedules and four analysis jobs were accepted. No failed/censored jobs, replacements, runner retries or clock reset; 1707 unused fresh slots remain unexecuted. One complete zero-deposit capture was retained. Both conditions completed 1024 calibration, 256 holdout and 128 fresh paired realizations.

Both fixed-design precision targets were missed: relative 95% mean interval half-width 8.05398% water and 8.01033% oxygen, versus 5%. Both complete paired ensembles pass the declared propagation support gates. Conditional fixed-intensity intervals and separate calibration-only/combined bootstrap sensitivity are retained for all metrics; combined intervals remain bounded exploratory approximations with unquantified tail contributions, not unconditional coverage or biological equivalence. No precision-qualified comparison is claimed and no additional sampling is authorized.

Evidence: .scratch/tests/2026-11-08__beam-on-fresh-source-calibration/06-study and 06-audit/local-report.md. Actual scientific startup inventoried 19602 prior inputs including 58 registries; 14594 new seeds are unique and disjoint, with all six control seeds reserved. All inventoried frozen/prior hashes match. 7088 native re-audits passed (3673 process, 2901 capture, 512 history, two identities); all 2901 energy ledgers balance. Postanalysis on a separate copy reproduces both original summaries, support tables, derived streams and capture ledgers byte-for-byte. The initial copied analysis exited 2 with missing/censored/duplicate-pair diagnostics. After logged path corrections, copied analyses exited 0 and reproduced the original outputs. Path relocation is the supported explanation; exclusive causation was not established. This was not a scientific retry. Original study before/after hashes show zero changes. No native workers remained at terminal checks. Preservation applies to inventoried evidence, not every unrelated file or an atomic filesystem snapshot.

READY for independent RESULT review. No ticket acceptance, source edits, new controls, staging, commit, push, PR, GPS state transition or delegation. Earlier 04/05 outcomes remain unchanged; no later experiment authorized.
