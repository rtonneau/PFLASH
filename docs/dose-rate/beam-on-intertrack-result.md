# Matched intertrack beam-on study: bounded result

The completed comparison does not resolve a nonzero contribution of inter-packet
coupling to the selected rate contrasts. All nine primary nominal simultaneous
intervals include zero. This is not evidence that coupling is absent: six of nine
precision targets were missed. The oxygenated-electron contrasts are more precise,
with an additional sparse-outcome limitation at 100 Gy/s.

The independent arm retains substantial rate differences. Thus the original
shared-history differences cannot be attributed to direct cross-track reactions
from those differences alone. The matched contrast estimates total coupling,
including common-environment depletion, not direct reactions alone. Molecular age
at observation still differs across rates in both arms; this study does not
separately identify age and other within-packet effects.

## Design and completed execution

User-approved [design](beam-on-intertrack-design.md); tooling commit `e4fe3f7`,
preparation baseline `0096d01`. New evidence root:
`.scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/prepared-study-v3/`.

- Reused 512 accepted Study06 shared histories, all 341 fresh physical packets
  and all 256 paired realizations (128 per condition). No new captures or arrival
  schedules; each packet remains one-use within its realization and arm.
- Ran 682 independent packet histories and 24 focused native controls. All 706
  jobs accepted; no failed, censored, replaced or retried histories. All controls
  passed before scientific execution. Empty realizations (44 water, 30 oxygen)
  remain in the denominators with zero contributions.
- Execution claim: 2026-11-09 09:11:00.5277757 UTC; fixed deadline
  11:11:00.5277757 UTC. Native execution plus per-job audits took 431.103 seconds.
  No clock reset or extension. Analysis and final preservation validation followed.
- Maximum launcher-reported job time 16.5132 seconds (cap 60); maximum committed
  process memory 26,517,504 bytes (cap 512 MiB); minimum free memory 7,010,750,464
  bytes (floor 3 GiB).
- Every independent packet retained its original position, precursor state,
  arrival and absolute endpoint: 20 microseconds + 2 ps at 100 Gy/s,
  2 microseconds + 2 ps at 1000 Gy/s. Exact packet energy, serialized expected and
  observed activation times, external instance mappings and endpoints were checked.
  Chemistry streams are independent; trajectories are not required to match.
- Shared-history reuse passed runtime, source, fixture, packet and endpoint
  checks. The private core hash remains
  `11AD89C931010589B56F32277CE0885397689E1BBF8C682FBFA2BBBD81BD574B`.
  No shared Geant4 source or install was edited.

The [independent arrival audit](beam-on-intertrack-arrival-audit.md) reconstructed
all 14594 Study06 seed assignments and all 256 paired schedules exactly, including
the first excluded draw and fixed-window inclusion. Oxygen's 183 arrivals versus
150.095749 expected are retained as the observed fluctuation. No count conditioning,
filtering or reroll defect was found. This conclusion is conditional on the
audited frozen prior-seed registry and standard MT engine implementation.

## Chemical means and contrasts

Counts are molecules per realization, not G values. Oxygen pool change is
final minus initial; negative means net consumption. Every independent packet
starts with the original full pool at the original concentration and volume.
The independent oxygen result sums each packet's **change**, never duplicated
initial inventories. Other recorded bulk components use the same accounting.

| Observable | Rate (Gy/s) | Shared mean | Independent mean |
|---|---:|---:|---:|
| Water aqueous electrons | 100 | 20.74219 | 20.67188 |
| Water aqueous electrons | 1000 | 22.21875 | 21.69531 |
| Oxygenated aqueous electrons | 100 | 0.12500 | 0.10938 |
| Oxygenated aqueous electrons | 1000 | 2.42969 | 2.42188 |
| Oxygen pool change | 100 | -35.25781 | -35.29688 |
| Oxygen pool change | 1000 | -32.81250 | -33.25781 |

Define C100 and C1000 as shared minus independent at each rate, and
D = C1000 - C100. D is the shared rate contrast minus the independent rate
contrast. Units throughout the next table are molecules per realization.

| Observable | Contrast | Estimate | Conditional 95% | Nominal simultaneous 95% | Calibration-plus-sampling sensitivity 95% |
|---|---|---:|---|---|---|
| Water aqueous electrons | C100 | 0.0703 | [-0.6563, 0.7891] | [-1.0000, 1.1094] | [-0.6648, 0.7854] |
| Water aqueous electrons | C1000 | 0.5234 | [-0.1406, 1.2109] | [-0.4176, 1.5313] | [-0.1374, 1.2388] |
| Water aqueous electrons | D | 0.4531 | [-0.4844, 1.4609] | [-0.8516, 1.9019] | [-0.4862, 1.4976] |
| Oxygenated aqueous electrons | C100 | 0.0156 | [0, 0.0391] | [0, 0.0547] | [0, 0.0399] |
| Oxygenated aqueous electrons | C1000 | 0.0078 | [-0.2188, 0.2422] | [-0.3047, 0.3438] | [-0.2221, 0.2449] |
| Oxygenated aqueous electrons | D | -0.0078 | [-0.2344, 0.2266] | [-0.3203, 0.3281] | [-0.2366, 0.2301] |
| Oxygen pool change | C100 | 0.0391 | [-0.7813, 0.8281] | [-1.1484, 1.1641] | [-0.7919, 0.8377] |
| Oxygen pool change | C1000 | 0.4453 | [-0.3672, 1.2969] | [-0.6719, 1.6832] | [-0.3685, 1.2961] |
| Oxygen pool change | D | 0.4063 | [-0.4922, 1.3047] | [-0.8438, 1.6719] | [-0.5006, 1.3201] |

Independent 1000-minus-100 mean differences are 1.02344 water electrons,
2.31250 oxygenated electrons, and 2.03906 oxygen pool change. Their paired
comparison with the shared rate differences is D above; these point differences
are not separate tests of significance or mechanistic partitions.

## Uncertainty and precision

Each condition uses 128 independent paired realizations, not 341 independent
packets. All arms and rates stay together in 20000 bootstrap resamples. Percentile
bounds use linear interpolation. The nine-primary family uses Bonferroni
probabilities 0.05/18 and 1-0.05/18. These are nominal approximate bootstrap
intervals, not guaranteed finite-sample coverage. Endpoint selection was informed
by the earlier shared outcomes, so this remains an exploratory follow-up.

The predeclared simultaneous half-width target was 0.5 molecule per realization.
All three oxygenated-electron contrasts meet it (half-widths 0.02734, 0.32422,
0.32422). Water-electron half-widths are 1.05469, 0.97440, 1.37675; oxygen-pool
half-widths are 1.15625, 1.17753, 1.25781. These six miss the target. The low-rate
oxygenated-electron contrast is sparse; its zero lower bound does not constrain
unseen rare events. Meeting precision is not an equivalence claim.

Study06 calibration half-widths remain 8.05% water and 8.01% oxygen; the prior 5%
target was not required here. Sensitivity resamples the 1024 calibration deposits
and the 128 units, applying an unnormalized Poisson weight once per realization's
contrast. It is separate from fixed-intensity conditional inference. All 20001
support rows per condition passed without clipping or discarded replicates:

| Condition | Minimum ESS | Maximum normalized weight | Maximum P(N > 8) |
|---|---:|---:|---:|
| Water | 123.2195 | 0.013534 | 0.000008539 |
| Oxygen | 122.4707 | 0.014996 | 0.000012827 |

Gates were ESS >=64, maximum weight <=0.10, and tail <=0.001, evaluated on
original units before bootstrap multiplicities. The sensitivity intervals are
approximate design sensitivity, not unconditional coverage; the chemical
contribution of unobserved N>8 histories remains unquantified.

## Mechanism limits

Removing other packets removes both direct cross-source reactions and coupling
through the shared finite oxygen pool. This two-arm intervention does not
quantitatively partition those mechanisms. An independent initial pool for each
packet is the deliberate control, not a conserved subdivision of the shared pool.
Adding its summed change to one initial pool is an accounting reference only.

The reused disjoint-ancestry diagnostic records two reactions, both at 100 Gy/s
(one per condition); it misses ancestry-free partners and downstream reactions
after ancestry masks overlap. It is not a complete measure of direct coupling.
Independent chemistry noise contributes to observed S-I even in singleton units.
Null-compatible estimates neither prove zero cross-track reactions nor exclude
effects at other doses, geometries or times.

This result applies to the 4-micrometer cube, 0.002-Gy nominal fixed-duration
Poisson design, accepted packets and patched private runtime. No biological FLASH,
production-scale feasibility, or beam-off mesoscopic conclusion follows.

## Evidence and review

The new root contains `jobs.tsv`, `streams.tsv`, `units.tsv`, per-job arguments,
instance maps, histories and audit logs, `outcomes.tsv`, execution timestamps and
status, `summary.tsv`, `unit-contrasts.tsv`, `support.tsv`, and provenance seals.
All frozen, input and original hashes passed before and after native execution.
Preservation covers inventoried files, not an atomic filesystem snapshot.

- Summary SHA-256: `ABA3121D232EE676C0131C4E6A52233A176BAA1FD02493942A9824BF52DA8DCD`.
- Frozen manifest SHA-256: `94784B9C4EC1807E325BE1A60EF725F9DD835BAD020F08F4BB3C5A4454A48383`.
- Unit contrasts SHA-256: `C3E70679DCF2B8453E47E297E48BE27059D7D52949BDC23CBA27078E905EF2AA`.

Three nonchemistry tests passed, including empty/duplicate/missing aggregation,
background deltas, sparse paired contrasts, one-ULP timing rejection, competing
execution claims and failed-publication evidence retention. Independent code and
scientific reviews identified timing tolerance, external control identity,
execution-claim ordering and sparse-flag issues; all were corrected before
chemistry and accepted by fresh review. Native controls then passed for empty,
same-seed singleton equivalence, independent singleton, late precursor activation,
repeat equivalence and two-source identity accounting in both conditions/rates.

The first preparation stopped on a documented later narrative declaration;
`prepared-study-v2` was superseded by reviewed corrections before chemistry.
Both remain preserved. Only `prepared-study-v3` ran chemistry. Earlier Study04/05
and other excluded data remain excluded. GPS state stays ignored. No push or PR.

Independent native-result/provenance audit found no blocker: all 706 jobs,
9,114 published unit rows and 147 summary means were checked; 72,396 inventory
entries rehashed with zero mismatches. Its scope and limitations are recorded in
the sibling `result-audit/audit.md`. This audit does not independently decode
packet bit serialization or attest every binary's build origin.

A separate scientific reviewer independently implemented the bootstrap in C++
without linking the analysis implementation. All 54 primary interval bounds
agree within 2.89e-15; all 40,000 resampled support rows agree within 1.36e-12.
The reviewer verified the tables, precision flags and interpretation and found no
blocker. Reproduction artifacts are in
`.scratch/tests/2026-11-09__intertrack-scientific-review/`. The sparse low-rate
oxygenated-electron contrast has only two nonzero paired differences.

The new `matched-study-v3-evidence.zip` beside the evidence root contains 7,086
files, each independently compared by SHA-256 with its live counterpart; zero
mismatches. Archive SHA-256:
`971C47D2A48A6E1FE4BFE5779C5FB7C5B4553CBEDC35DA9245D1E61819BDF13D`.
It preserves the native run and analysis; subsequent independent review artifacts
are separate supplements. Study06's original archive remains unchanged.

The bounded study is accepted as complete, audited and honestly reported, not
as proof of no coupling or attainment of all precision targets. Final nonchemistry
verification passed 3/3 tests. No additional sampling is authorized by this result.
