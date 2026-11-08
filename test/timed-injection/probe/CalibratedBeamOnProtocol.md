# Frozen fresh-source calibration protocol v1

This is the deterministic protocol for the approved
`.claude/plans/beam-on-fresh-source-calibration.md`. Freeze this file, source,
macros, binaries and the exact generated manifest before physical study sampling.
Never reuse an output directory or replace a failed attempt. Controls and builds
are separately labelled and are not study observations.

## Estimands and fixed budget

Two conditions: oxygen-free water and the existing 21% oxygen fixture. Geometry:
4 micrometre water cube, half-side .002 mm, mass 6.4e-14 kg, 500 keV electrons,
precursor age 1 ps. Exactly 1024 calibration and 256 holdout attempts per condition.
All complete zero deposits enter denominators. Invalid/aborted/truncated captures
are retained and prevent complete-sample claims. Require process success, complete
validated packet, energy ledger balance at .5 MeV (1e-8 MeV tolerance), private
module provenance and the final validation marker together.

At the fixed calibration size, the two-sided 95% Student-t mean interval must have
relative half-width <=.05. Nonpositive means fail; a precision target miss remains
explicit and can only support exploratory comparisons. No extra sampling. Freeze
the calibration summary and its hash before launching holdout. Holdout is excluded
from calibration and bootstrap pools; report its mean/SE, zero fraction, quantiles,
and calibration-minus-holdout Welch interval without tuning.

128 paired realizations per condition. Nominal dose .002 Gy, rates 100 and 1000
Gy/s, fixed exposure T=.002/rate seconds, observation T+2 ps. Sample exponential
waits at rate lambda=rate*mass/(frozen mean MeV*1.602176634e-13). Generate one
normalized schedule per realization and scale it to both exposures. Selection
is unused: fresh arrival k always uses fresh packet k. At most eight sources;
the ninth censors the entire pair without resampling or count conditioning.
Every arrival has one independent capture, used once within the realization and
shared only by its paired arms. Empty schedules retain two empty chemistry
histories; an accepted calibration packet supplies geometry but is not injected.
Native fixed-mode history means describe its fresh library, not calibration;
the external `.calibration` sidecar identifies the frozen mean and schedule job.

Absolute ceilings: 4608 physical attempts, 512 chemistry histories, 256 schedule
jobs, serial execution, four hours from first study process. Every launcher gets
min(60 seconds, remaining global time); never reset the clock per condition.
512 MiB per-process cap and 3 GiB free-system-memory floor. Preserve every planned
unexecuted/failed/censored row. No replacement, extension, successful-subset
intervals or retries. Repeated identical technical failure twice blocks further
launches. Missing/failed/censored pairs withhold complete-ensemble intervals.

## Exact manifest and random streams

`CheckCalibratedBeamOn plan` inventories all 2304 possible capture IDs per condition
in order: capture0..1023 calibration, capture1024..1279 holdout, capture1280..2303
fresh. For fresh capture n, realization=(n-1280)/8 and arrival=(n-1280)%8.
Each capture has separately allocated physical transport, face-position and
precursor-chemistry seeds. After a condition's captures, allocate arrival,
paired-history chemistry and unused selection seeds for pair0..127, then its
bootstrap root. Water precedes oxygen. Allocate consecutive unused integers from
100000000, skipping the retained historical set; all new streams are collision
audited. History chemistry is deliberately shared only between rate arms of the
same paired realization. Bootstrap derives separate calibration and pair streams
by the versioned analyzer's SplitMix64 domains and records them in its summary.

Prior `.args`, `.history`, `.calibration`, histories/runs/results CSV, seed/ledger and protocol-at-run evidence
under earlier `.scratch/tests` session roots is hashed. Conservatively reserve all
positive integral tokens <=2147483646; repeated prior paired seeds are deduplicated
by value. The active session is excluded. Keep the exact prior seed set, input
hash list, stream table, planned capture ledger, pair/packet indices and manifest
hashes with results. `processes.csv` records attempted launches, remaining-time
deadlines, outcomes and elapsed times. `capture-ledger.txt` retains all planned
rows; unused fresh slots remain unexecuted and are not physical observations.

## Uncertainty and support gates

Primary contrast: paired 1000-minus-100 species counts and oxygen pool change;
also report actual dose and source count. Union species across complete outputs;
zero-fill only truly absent species. At mu0=.002*mass/meanEdepJ, each bootstrap
calibration mean implies mub and weight exp(mu0-mub)*(mub/mu0)^N. Apply that weight
once to the pair, use unnormalized means, never clip or renormalize to N<=8.

2000 preseeded replicates independently resample calibration events and paired
realizations. Report fixed-intensity conditional sampling, calibration-only and
combined sampling/calibration design sensitivity separately. Combined intervals
are bounded exploratory approximations with unquantified tail contributions,
not unconditional coverage guarantees or biological equivalence.

Require nominal and every replicate P(N>8)<=.001, ESS=(sum w)^2/sum(w^2)>=64,
and maximum normalized weight<=.10. ESS is calculated on the original independent
128 pair weights before resampling indices. Report exact overflow probabilities
and probability of any overflow in 128. Nonpositive bootstrap means, numerical
overflow/underflow or any failed support diagnostic withhold propagated intervals;
never drop draws or increase budgets. Preserve all 2000 diagnostic rows.

## Verification and runtime

Use the validated private IRT_syn runtime from session
2026-11-08__beam-on-irt-beam-off-meso. Its G4processes_core.dll SHA256 is
11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B.
Hash all runtime DLLs; require loaded G4 modules to come from that exact install.
Freeze source/executable/macro inputs and verify post-run hashes. Old evidence
and runtime are read-only. Synthetic checks exercise the actual orchestration
function with fake launch outcomes, plus real C++ manifest, activation, packet,
energy and statistics validators. They launch no physical study jobs.
