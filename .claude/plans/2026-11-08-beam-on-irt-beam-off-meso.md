# Beam-on IRT_syn and one-way beam-off mesoscopic implementation plan

> Use subagent-driven development with one implementation writer and fresh independent review. User approved this architecture in the conversation; do not reopen the rejected spatial-coupling or checkpoint designs.

**Goal:** Establish a bounded shared IRT_syn irradiation reference, then evaluate an optional source-free mesoscopic tail.

## User steering: current authorized scope

The user explicitly directed this run to focus on beam-ON IRT_syn with Poisson-distributed track arrival intervals. The optional beam-off mesoscopic task below is deferred and must not be implemented in this run. No beam-off accuracy decision is required now.

The beam-on driver must generate a homogeneous Poisson process for each requested constant dose rate over a fixed exposure duration. Use an independent source random stream, calibrated rate `doseRate * massKg / meanDepositedEnergyJ`, and retain each sampled packet's correlated energy and spatial precursor cloud. Track count and actual dose fluctuate; do not stop at a fixed packet count or nominal deposited dose. Geometry-specific calibration and its sampling uncertainty must be explicit. Deterministic schedules remain mechanism controls. Validate Poisson count/interval behavior independently of chemistry, fixed-window exclusion, zero-arrival histories and source/chemistry random-stream separation. Predeclare a bounded dose-rate comparison with dose/duration conventions and independent chemical histories; finite-library reuse limits and statistical versus physical uncertainty must remain visible. Do not claim a measured dose-rate effect merely from differing schedules or change the full-dose resource prohibition.

**Architecture / approved design:** All scheduled sources and old molecules evolve in one native particle system throughout irradiation and post-source relaxation. Only after the final precursor activation and resolution of pending precursors may the entire state transfer once to mesoscopic chemistry. There is no return to particles and no later source. Pulse-train gaps are not final beam-off. Exact trajectory identity is not a scientific acceptance requirement.

**Tech stack:** C++20, configured Geant4 11.4.3, isolated MSVC/Ninja probes, existing capped Windows Job Object launcher and audit runners.

## Constraints

- Baseline 7b5b2bd; preserve all prior evidence and ignored GPS state. No shared Geant4 source/install changes, production integration, full-dose run, PR or push.
- Paths resolve through project config and DEV_DIR/G4_ROOT. Read actual source implementations and examples before changing simulation behavior.
- Extend only isolated probes and documentation. New scratch under `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso`; never overwrite older builds or logs.
- Four compiler jobs; serial runtime diagnostics initially. Use hard Job Object limits: 512 MiB committed-private memory and 60 seconds per bounded chemistry history, 3 GiB system-free floor. Kernel build uses existing 4 GiB/process, one-hour cap. Caps are fixed before execution; censored runs are failures/limits, never silent successes.
- Source selection/times and chemistry randomness remain separate. Count independent physical histories separately from chemistry replicas. Existing finite packet libraries are not an unbiased production beam.
- Parent owns acceptance and commits. Independent reviewers do not mutate files or run writing tests.

## Task 1: Integrated runtime and finite beam-on reference

- [x] Audit existing `test/timed-injection/probe` schedule/history/retention harnesses and `test/molecular-finder` controls. Record exact source/patch provenance and pristine/shared hashes.
- [x] Prepare a new private source/build/install combining the validated finder correction with required native source-contact and reflecting-boundary changes. Resolve overlaps explicitly; never assume separately validated patches compose correctly. Preserve standalone accepted builds.
- [x] Reuse finder brute-force, native coincidence and retained-guard controls plus source activation controls against this combined runtime. Add integration regressions only where their composition creates a new failure mode.
- [x] Implement a small finite-schedule driver (proposed `BeamOnHistory.cc` with focused helper if necessary) and bounded runner/analyzer alongside existing probes. Import a variable ordered list of compatible packets; keep one shared native history; retain per-source energy and activation ledgers, actual bulk counts, species/live-counter checks, ancestry and private-runtime provenance.
- [x] Validate empty, single, simultaneous, close, delayed and final-boundary sources. Reject invalid/unsorted or incompatible input explicitly. Distinct coincident sources must use distinct IDs and preserve native contacts. Include oxygen-free and oxygenated fixtures and engineered depleted-pool continuity.
- [x] Predeclare a small independent-packet ensemble and bounded 2/4/8 packet cost staircase before executing; reuse compatible originals once per cluster, no unacknowledged exact-copy beam model. Fix seeds/endpoints and uncertainty estimands in a tracked protocol. Measure initialization separately from chemistry where possible, plus hard-cap private-memory peaks.
- [x] Run appropriate existing native-path regression controls. The rejected copied adapter remains a separately labelled failed diagnostic, not the candidate or an expected-success gate.
- [x] Report the supported bounded native regime and resource limits. Do not infer full 0.5 mm/10 Gy feasibility from small fixtures.

## Task 2: Optional source-free mesoscopic tail (DEFERRED by user; not current work)

- [ ] Proceed only after task 1 passes correctness. Use matched finite source schedules and particle reference runs continuing to the same final observations.
- [ ] Implement exactly one whole-state handover after the final activation plus a positive relaxation interval, preserving absolute clock, live species/configurations, actual bulk counts and source/energy ledger. Assert no delayed source or unresolved precursor remains; reject an input that schedules a source after transfer.
- [ ] Test exact handover budgets and observation order, including quiet queues and observations at the transition. Do not require total molecule count constant during reactions.
- [ ] Before chemical comparisons, fix delay/resolution grids, observations, independent physical clusters and replica seeds. Choose actual nonzero transfers; no zero-transfer substitute. Resolution must pass native reaction-radius checks.
- [ ] Obtain the user scientific observable/error target (question pending). Develop an explicit error budget tied to that target; never retrofit a margin to results. If no justified numeric margin is available, quantitative exploratory differences may be reported but no scientific equivalence GO is issued.
- [ ] Compare species yields and actual oxygen change, cross-source controls and post-source convergence with uncertainty; distinguish model bias from sampling imprecision. Early packet-age and finite-library limits remain explicit.

## Task 3: Review and local delivery

- [x] Write `docs/dose-rate/beam-on-irt.md` with supported regimes, limitations, exact commands/protocol, hashes, raw evidence paths and next decision. Keep pass/fail/untested distinct.
- [x] Freeze source/docs; fresh independent code and scientific review. Resolve actionable findings with targeted verification, then review changed scope again.
- [x] Parent commits only validated work; preserve ignored evidence locally and record no-publication restriction.

## Review focus

Patch composition and loaded DLL provenance; future sources executed early or dropped; premature handover while a delayed precursor remains; reset of depleted bulk; unsupported equivalence or full-scale claims from limited sampling. Each is covered by the corresponding integration, schedule, transfer, bulk or analysis checks above.


## Delivery record

Task 1 completed as a bounded exploratory beam-on reference; Task 2 remains deferred. See docs/dose-rate/beam-on-irt.md for the composed private runtime, 114 protocol histories, runtime controls, uncertainty and limits. Independent code and scientific reviews agreed on one analyzer omission. The parent corrected it, verified a failing-then-passing synthetic regression plus incomplete-arm checks, regenerated summaries without overwriting originals, and reran both pure CTests (2/2 passed). A fresh read-only correction review returned no findings. No shared Geant4 changes, PR, push, production integration or full-dose simulation.
