# Fresh-source calibration design — proposed for approval

Baseline: ffe1a57; continuation of docs/dose-rate/beam-on-irt.md.
Scope: isolated 4 micrometre cube, 500 keV physical captures, oxygen-free and existing 21% oxygen fixtures; private validated IRT_syn runtime. No production integration, mesoscopic tail, shared Geant4 changes, push or PR.

## Agreed calibration budget
Per oxygen condition: exactly 1,024 preassigned calibration attempts and 256 disjoint holdout attempts. Target: two-sided 95% Student-t interval half-width / calibration mean <= 0.05 at the fixed final sample size. Report mean, sample SD, SE, interval, zero fraction and quantiles. Bootstrap percentile intervals provide a distribution-shape sensitivity check, not a replacement success criterion. No optional stopping or enlargement after seeing results. Nonpositive means fail calibration. If complete captures miss the 5% target, report target missed; comparisons may remain exploratory but cannot be presented as precision-qualified.

Attempt IDs and independent seed streams are assigned before sampling. Preserve zero-deposit complete events in every denominator. Require successful process status, validation marker, complete packet and consistent energy ledger together; packet-file existence alone is insufficient because capture can write before a later validation failure. Reject truncated, aborted, energy-unbalanced or incomplete captures and retain their diagnostics; never replace them. Any invalid planned capture prevents claiming complete-sample precision or proceeding to an accepted beam comparison for that condition. Unexecuted attempts remain explicitly unexecuted.

Freeze calibration before examining holdout results. Holdout never changes the rate calibration, precision target, seeds or budget. Report independent holdout mean/SE and calibration-minus-holdout difference with uncertainty, plus distribution/zero-fraction diagnostics. A mismatch is reported, not tuned away. Earlier small banks are context only.

## Proposed fresh comparison
128 independent realizations per condition, paired at 100 and 1,000 Gy/s; nominal dose 0.002 Gy. T = dose / doseRate remains fixed. Exponential arrivals in [0,T), lambda = doseRate * mass / mean deposited energy. Keep mass 6.4e-14 kg, 1 ps precursor age and observation T + 2 ps from the tested fixture, verified against actual geometry and capture metadata.

Every arrival gets a newly captured packet, never a calibration/holdout packet. Use each packet once within a realization; share only between its paired rate arms. Realizations have disjoint physical capture IDs. Keep source-position, physical-transport, arrival, chemistry and statistical-resampling RNG streams separate with a collision-audited seed ledger. Preserve empty histories. No count fixing, dose stopping, resampling failed histories or energy-based packet selection.

Maximum eight sources per realization. A ninth arrival censors that realization rather than changing its Poisson law. Any failed/censored planned comparison prevents reporting complete-ensemble intervals for that condition; retained valid outcomes remain descriptive. Caps remain 512 MiB/process, 60 seconds/process, 3 GiB free-system-memory floor, serial execution. Maximum planned physical attempts: 2*(1024+256+128*8)=4608; chemistry histories: 2*128*2=512. Generate one normalized schedule per realization and scale its times for the two rates: at most 256 schedule-generation jobs; controls/builds are separately logged. Study execution has a four-hour wall-clock ceiling, enforced by passing min(60 seconds, remaining study time) to each launcher and starting no process after expiry; unfinished work is reported without extension or replacement. No precision claim for chemistry is implied by 128 realizations.

## Proposed uncertainty propagation
Primary comparison: paired 1000-minus-100 Gy/s differences in final species counts and oxygen-pool change; include union of observed species, zero filling only genuinely absent species in complete outputs. Also report actual deposited dose and source count.

At the frozen calibration mean, mu0 = nominalDose * mass / meanEdepJ. Resample the 1024 calibration events with replacement to obtain a mean and corresponding mub. For a complete realization with N arrivals, the Poisson likelihood ratio is exp(mu0-mub)*(mub/mu0)^N. Conditional on N, normalized arrival times and fresh packet distributions are unchanged; thus apply the same weight to both paired arms. Do not multiply the weight twice. Use unnormalized importance-weighted means for the primary expectation estimate.

Use 2000 preseeded bootstrap replicates: independently resample calibration events and complete paired realizations, recompute weights, and calculate paired mean contrasts. Report percentile intervals as approximate combined sampling/calibration design-sensitivity intervals: they describe variation when the calibrated intensity changes, not uncertainty of the chemistry expectation at the already deployed fixed intensity. Report conditional-only sampling intervals for that fixed intensity and calibration-only resampling diagnostics separately. Do not add the components as though independent or count bootstrap draws as physical observations. Holdout is excluded. Validate the likelihood-ratio implementation against analytic compound-Poisson moments and direct schedule Monte Carlo.

Calculate and report exact Poisson P(N>8) at nominal calibration and every bootstrap calibration, including probability of at least one overflow in 128 realizations. This tail is not represented by the capped chemistry outcomes; ESS cannot establish support for it. Require P(N>8) <= 0.001 throughout as an additional operational acceptance bound; otherwise withhold combined intervals. Even below that threshold, label intervals as bounded exploratory approximations with unquantified tail contributions to chemistry observables, not unconditional coverage guarantees. Never renormalize to N<=8 or claim absence of bias from observing no overflow.

Report effective sample size (sum w)^2/sum(w^2) and largest normalized weight for each replicate. Require ESS >= 64 and maximum normalized weight <= 0.10 for every replicate to publish the combined interval; otherwise label propagation unsupported under this fixed budget. Do not clip weights, drop problematic replicates or enlarge the ensemble. A nonpositive bootstrap mean is an explicit unsupported outcome. These support thresholds are operational diagnostics, not coverage guarantees. No bootstrap interval corrects model bias or establishes biological equivalence.

## Alternatives and validation
Chosen proposal: fixed calibration/holdout samples plus Poisson importance reweighting. Sequential precision stopping would need sequentially valid intervals; nested new chemistry ensembles for each calibration draw would substantially increase cost. Both are outside this bounded proposal.

Before sampling, freeze a tracked protocol, exact seed manifest, executable/source/macro hashes, estimands and budget. Verify actual Geant4 .cc paths and existing capture hooks before altering simulation code. Reuse private runtime only after provenance verification. Test zero deposits, truncated/aborted rejection, split/seed disjointness, Poisson count/time law, stream invariance, packet uniqueness, paired schedules, analytical reweighting and incomplete-data refusal. Audit each attempt and activation/energy ledger. Preserve old evidence; use the new session scratch directory. Report target met/missed, failures, achieved sample sizes and limitations without substituting a successful subset.

## Implementation outline after approval
1. Capture ledger and validation with deterministic disjoint split/seed allocation.
2. Calibration/holdout statistics and frozen protocol plus budget enforcement.
3. Fresh ensemble runner and uncertainty analyzer, targeted regression checks.
4. Bounded execution, independent evidence/code review, documentation and local commit.



## Execution status — 2026-11-08
Ticket04 launched once and was interrupted for an operator preflight path-isolation error after 12 accepted water captures and one unadjudicated attempt. No replacement or extension. Both conditions are incomplete; all comparison and propagation claims are withheld. See docs/dose-rate/beam-on-irt.md for evidence and incident details. Parent review and acceptance remain outstanding.

