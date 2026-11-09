# Bounded beam-on IRT_syn protocol

Fixed before running this protocol: use the separately captured 4 µm-cube
oxygen-free and 21% oxygen packet libraries from the earlier feasibility work.
Each packet retains its own deposited energy and correlated precursor cloud.
The configured world is the matching 4 µm cube; do not use the 500 µm
calibration. The capture age is 1 ps, so a primary arriving just before the
physical exposure end can activate after it. The final native observation is
2 ps after the exposure end and all scheduled activations must precede it.

The source sampler draws exponential interarrival times in `[0,T)` and samples
whole packets uniformly with replacement. Arrival, selection and chemistry
seeds are independent. It never fixes the number of sources or stops when a
target dose is attained. `T = 0.002 Gy / doseRate`; the rates are 1, 10, 100
and 1,000 Gy/s. The actual energy and dose fluctuate, and the 0.002 Gy is an
expected dose conditional on each empirical calibration. These are exploratory
physical-rate inputs, not demonstrated therapeutic irradiation conditions.

For each oxygen condition form four disjoint physical-packet clusters. In the
oxygen-free library cluster `c=0..3` contains `library[4c]` through
`library[4c+3]`; in the oxygenated library it contains `library[2c]` and
`library[2c+1]`. Each cluster's own uniform-sampling mean calibrates its rate.
For each cluster and rate run two source/chemistry replicas `r=0,1`, giving
64 planned histories. First run cluster 0, replica 0 for each condition and
rate. If a smoke history is capped, times out or fails, preserve it and do not
run the remaining ensemble for that condition/rate; report the arm as censored
or failed, without estimating an unbiased mean from the remaining runs.
Diagnose non-cap failures and preserve the original evidence; if a defect is
corrected, a fresh validation and fresh protocol run are required. Do not
increase the caps or rerun a capped history as if it passed. Set arrival seed `100001 + 10000*oxygen + 10*c + r`, selection seed `200001` plus
the same offsets, and chemistry seed `300001` plus the same offsets. Use these
same seeds across all rates for a given cluster and replica: the fixed-nominal-
dose arms have paired source counts, packet choices and energies while arrival
times rescale. Their Poisson marginals remain unchanged; no count is conditioned
or fixed. Rate contrasts are paired within physical clusters and no exact
chemistry trajectory identity is assumed. Every history, including an empty, failed or
censored one, is recorded. More than eight arrivals is a censored history; do
not resample or treat survivors as an unbiased ensemble.

Compute per-history actual dose, source count, surviving species and oxygen
change. Average replicas within each physical cluster, then use the four cluster
means for an exploratory mean and t(3) interval if all required runs complete;
do not treat molecules or chemistry replicas as independent physical captures.
These intervals cannot correct finite-library bias. Exposure and molecular-age
distributions differ by design in this delivery-rate comparison; attributing
any yield difference specifically to inter-track interactions requires matched
single-track controls and reaction evidence. The tiny cluster sample does not
justify an equivalence or biological-effect claim.
Uniform sampling with replacement also repeats the exact captured spatial
track whenever an index recurs; prior diagnostics show this can bias
inter-track chemistry. These empirical-bank histories are mechanism and
reference checks, not unbiased beam-physics estimates.

As a distinct-capture physical-gap control, use two disjoint physical pairs
`library0,library1` and `library2,library3` for each oxygen condition. Each
pair is imported once, at 0 and at a fixed gap of `1e6` or `1e7` ps. Set
exposure to gap + 1 ps and observation to gap + 3 ps, with chemistry seeds
95001 and 95002 for each pair/gap. These microsecond-scale gaps are illustrative
of the 4 µm calibration near 1,000 and 100 Gy/s, respectively. They are fixed
two-source controls, not Poisson histories or rate-effect estimates. Run the
first pair/first seed as a smoke per condition/gap; if it is capped or fails,
retain the outcome and skip that control arm. Report these two pairs
descriptively; their size does not support a quantitative physical inference.

For an additional fresh-per-arrival Poisson control, use four source
realizations per oxygen condition at 100 and 1,000 Gy/s, paired across rates
with the same normalized arrival stream. Each realization has a fixed nominal
dose of 0.002 Gy and the same half-open Poisson window and eight-source cap.
Use the existing full 4 µm calibration banks (16 oxygen-free or 8 oxygenated
`library` packets), whose previously captured mean deposits are respectively
0.0004583859867103157 MeV (SE 0.0000856204856481622 MeV) and
0.000556467579215713 MeV (SE 0.00016694983712935 MeV); the mass is
6.4e-14 kg. These constants calibrate lambda, not the fresh outcome packets.
Arrival seed is `400001 + 10000*oxygen + realization`, shared across rates.
Every generated arrival receives one independently captured 500 keV physical
packet, used exactly once within that history. For arrival `k`, use capture
physics seed `501000 + 10000*oxygen + 1000*realization + 101*k`, capture
source seed `601000` plus the same offsets, and capture chemistry seed `701000`
plus the same offsets. Use `801000 + 10000*oxygen + 1000*realization` as the
shared-history chemistry seed across rates. The same fresh realization and
packets are paired across its rate arms. Retain all captures, including zero
deposit, and every failed or censored capture/history; never replace a packet
based on its energy. Capture and history processes use the same hard resource
caps. The first realization is a smoke for each condition/rate; an arm stops
after a capped or failed smoke, with diagnostics preserved. Four realizations
are exploratory and calibration uncertainty and source-window age differences
remain limitations; this is no scientific equivalence or full-dose claim.

For a cost staircase, use distinct `library0` through `library(N-1)` exactly
once for `N=2,4,8`, at `0, 0.25, ...` ps; exposure is 10 ps and observation
12 ps. Run three chemistry seeds 93001, 93002 and 93003 per N, for both oxygen
conditions. These deliberately compressed timing controls exercise overlap;
they are not physical-rate estimates or full-dose extrapolations. The driver
preconstructs all future tracks in delayed lists: its peak includes scheduled
input as well as active chemistry, and it is not a streaming implementation.

The stream-invariance mechanism check additionally uses the first two
oxygen-free packets, 10 ps exposure and an intentionally compressed
`4e8 Gy/s`: arrival seed 1733 and selection seed 1741 are held fixed while
the chemistry seed changes from 1729 to 1799. A third run keeps arrival seed
1733 and uses selection seed 1777. The first two must have identical source
times, packet choices and energies; the third must have identical source times.
An empty or capped control is reported as a failure rather than silently
choosing another seed.

Run each serial history in a Windows Job Object with 512 MiB committed-private
memory per process, a 60 s deadline and a 3 GiB free-system-memory floor.
Preserve every log, cap notification, exit reason, module inventory and measured
job peak under the new task scratch directory. A cap or timeout is a limit,
not success. Capture initialization and chemistry times separately; neither is
a scaling forecast. Run source/finder/boundary controls against the composed
private runtime before interpreting any history.
