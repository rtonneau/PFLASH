# Matched intertrack mechanism study — proposed design

Status: approved by the user in full; no chemistry launched in tooling ticket. Baseline: 0096d01.

## Question and scope

Estimate whether removing other injected packets changes the Study06 beam-on
rate contrast. Reuse its accepted calibration and 128 fresh realizations per
condition, including all empty realizations. No new physical captures or arrival
draws. The roughly 8% calibration half-width is retained, not an eligibility
failure. Geometry, chemistry, packets, precursor ages and rate-specific absolute
endpoints remain those of Study06. No mesoscopic tail, production or biological
claim, shared Geant4 edit, push or PR.

## Arrival audit before chemistry

Independently reconstruct intensity from frozen deposit means, mass and energy
conversion; reconstruct seed domains and all exponential draws through the first
excluded arrival. Compare every saved schedule, including normalized paired
times, half-open window inclusion, overflow and empty cases. Inspect runner paths
for seed reuse, count conditioning, retries and filtering. Preserve original
bytes. A correct unusual ensemble is retained; a defect blocks chemistry and
requires an affected-result inventory and revised design. Audit code and results
live separately from Study06 evidence.

## Arms and interpretation

S: all packets in one native shared IRT_syn history.
I: each packet alone, with its original arrival, precursor timing, position,
environment and absolute endpoint. Sum its chemical contributions per realization.
Use independent chemistry streams; identical trajectories are not required.
Preserve external instance identity even where native local track IDs differ.

Reuse S histories only after checking runtime and binary identity, fixture bytes,
packet payloads, schedules, endpoints, complete activation and energy ledgers.
Otherwise stop to explain incompatibility and revise the bounded run declaration;
do not silently substitute new shared histories.

Each oxygenated independent history has its own full initial pool B0 at the
original concentration and volume. Compare shared delta B = Bfinal - Binitial
with sum_j(Bfinal,j - Binitial,j). Positive consumption is minus this delta.
Never sum the initial inventories. B0 plus the summed deltas is an accounting
reference, not a physically evolved shared pool. Apply the same delta rule to
every recorded bulk component; keep tracked species separate. Check unchanged
pH-buffer behavior against source. Empty contributions are zero.

S minus I measures total inter-packet coupling, including direct reactions,
their downstream consequences and coupling through a common finite environment.
It is not a direct-reaction estimate. Report existing disjoint-ancestry reaction
counts separately, with their missing-ancestry and downstream limitations.
Two-arm controls cannot quantitatively partition direct and depletion effects.
The coupling interpretation concerns the expectation: individual differences
also contain independent chemistry noise, even in single-source realizations.
A third bath-clamping or reaction-suppression intervention is deferred because
it changes the native mechanism and requires additional validation.

## Predeclared endpoints and estimands

Observe at 20 microseconds + 2 ps for 100 Gy/s and 2 microseconds + 2 ps for
1000 Gy/s. Within a rate all arms use the identical absolute endpoint; never
restart a packet's clock at zero. The changing molecular ages across rates are
part of the protocol and are present in both arms.

Primary: final aqueous-electron count per realization in each condition and
oxygen bulk-pool change per oxygenated realization. For each of these three
condition/observable combinations estimate C_r = mean(S_r - I_r) at both rates
and D = C_1000 - C_100. Thus nine primary estimates. Other species, ancestry
counts and depletion fractions are exploratory diagnostics.

The independent sampling unit is the full paired realization, not a packet,
molecule or reaction. Retain all 128 units per condition. Use 20000 paired
nonparametric bootstrap replicates with frozen, disjoint analysis seeds; retain
all rates and arms together during resampling. Report ordinary 95% intervals and
Bonferroni simultaneous 95% intervals for the nine primary estimates (individual
coverage 1 - 0.05/9). These are nominal, approximate bootstrap coverage levels,
not finite-sample guarantees. Use percentile bounds with linear interpolation
at p*(B-1); half-width means (upper-lower)/2. Endpoint selection is informed by
the already observed Study06 results, so this is an exploratory follow-up even
though its new comparisons are declared before independent chemistry.
No significance-based acceptance or outcome selection.
Adjusted percentile probabilities are 0.05/(2*9) and 1-0.05/(2*9).
Flag sparse/degenerate bootstrap intervals as unable to bound unseen events.

Precision target: simultaneous interval half-width at most 0.5 molecule per
realization for each primary contrast. This resolves an average change of one
molecule with useful precision without unstable relative errors near zero.
It is a reporting target, not a stopping rule, equivalence margin or promised
power. Failure gives an inconclusive estimate, without budget extension.

Report conditional intervals at the deployed intensity as primary. Separately
propagate frozen-calibration uncertainty using Study06-style unnormalized
Poisson likelihood weights, resampling the calibration bank and realization
units. Report this as approximate design sensitivity, not unconditional coverage.
Predeclare the inherited support gates: ESS >=64, largest normalized weight
<=0.10, P(N>8)<=0.001, checked on every replicate without clipping or dropping.
Evaluate weight support over the original 128 realization units before applying
bootstrap multiplicities, consistently with Study06; do not interpret repeated
bootstrap indices as new independent support.
Apply each likelihood weight once to the full realization contrast, never
separately to its packets or rate arms.
If any gate fails, report sensitivity unsupported rather than successful-replicate
intervals. Explicitly retain the unquantified overflow-tail contribution.

## Fixed budget and failures

Proposed maximum: 682 independent packet histories (341 packets times two rates),
24 native validation jobs, zero new shared scientific histories, zero new
physical captures or schedules. Serial execution, 60 seconds and 512 MiB
committed-private memory per job, 3 GiB free-memory floor. One absolute two-hour
chemistry/control execution deadline, established before the first native job;
no reset or automatic extension. Audit, build and review do not consume this
execution deadline. Reserve all prior seeds, including unused reserved slots,
and freeze new chemistry and analysis streams before launch.
Freeze the complete scientific and validation job manifest with external
(condition, realization, arrival, rate) identities before the first native job.
All controls must pass before ensemble launch; their time uses this same budget.

Empty realizations remain exact zero contributions and contrasts. Source-count
overflow is a censored unit, never a replacement or a truncated successful run.
Any failed, missing, timed-out or censored required history makes its realization
incomplete. Preserve every planned unit and job outcome; do not compute ensemble
chemistry estimates from the successful subset. Report the incomplete study and
only explicitly labelled validation/availability diagnostics. No automatic retry.
This deliberately withholds ensemble inference for both conditions if either
condition is incomplete; a complete condition is not a substitute study.

## Implementation, validation and delivery

Use new isolated C++ probes and new protocol files; retain frozen originals.
Reuse the validated private runtime after hash and loaded-module checks.
Focused controls precede the ensemble: empty water/oxygen, single packet with
nonzero arrival, late precursor activation, exact energy/identity accounting,
synthetic aggregation with duplicated backgrounds, and negative controls for
missing/duplicated instances, overflow and incomplete output. A same-seed
single-packet equivalence check is a harness check only, not an ensemble rule.
Independent chemistry seeds make the scientific single-packet contrast noisy.

Obtain independent code and scientific review before the ensemble and review
results afterward. Preserve immutable evidence and ignored GPS state. Commit
validated task-owned code, protocol and report locally. GPS finish/publication
remains deferred under the explicit no-push/no-PR constraint.

