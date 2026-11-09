# Fresh-source calibration study (Study06): accepted result and limits

This is the consolidated record of the one accepted bounded execution of the
fresh-source calibration design. It extends the isolated beam-on reference in
[beam-on-irt.md](beam-on-irt.md) (fixture, source contract, runtime validation
and the per-attempt incident narrative). Where an older section there or in
`.claude/plans/` disagrees with this page, this page wins and the older text is
marked as superseded. The next-session starting point is the separate
[handoff](beam-on-fresh-source-calibration-handoff.md).

**Bottom line.** The study ran to completion once, inside its budget, and every
planned job was accepted and audited. It did **not** reach its precision goal:
the calibration mean is known to about 8% (relative 95% half-width), not 5%, in
both conditions. Every comparison below is therefore exploratory. Several
1000 minus 100 Gy/s differences have conditional intervals that exclude zero,
but the two arms differ in observation age as well as in track spacing, so they
are not shown to come from inter-track reactions.

## 1. What was run

| Item | Value |
|---|---|
| Fixture | Isolated 4 µm water cube, mass `6.4e-14 kg`; macros `MesoFixture4um.in` (water) and `MesoFixture4umOxygen.in` (21% dissolved O2) |
| Source packets | Physically captured 500 keV electron precursor packets; each capture's energy ledger balances to 0.5 MeV; precursor age 1 ps |
| Delivery | Fixed-duration Poisson arrivals, nominal dose 0.002 Gy, at most 8 arrivals per history |
| Rates | 100 Gy/s (exposure 20 µs) and 1000 Gy/s (exposure 2 µs); one normalized schedule per realization is scaled to both rates |
| Observation | Exposure end + 2 ps |
| Chemistry | One native IRT_syn history per realization and rate, in the private Geant4 11.4.3 runtime |
| Calibration | 1024 attempts per condition set the arrival intensity (frozen before the fresh stage); 256 disjoint holdout attempts per condition, never used for calibration or resampling |
| Fresh comparison | 128 realizations per condition, each arrival a newly captured packet used once, shared only by the two rate arms of that realization |
| Estimand | Paired 1000 minus 100 Gy/s difference of final species counts and of the O2 pool change (`bulk_after - bulk_before`, molecules per history) |
| Design rule | Student-t 95% half-width of the calibration mean at most 5% of the mean |

Species values are final counts of tracked molecules per history, not G values.
The radical marker `°` is Geant4's. The protocol is frozen in
[`CalibratedBeamOnProtocol.md`](../../test/timed-injection/probe/CalibratedBeamOnProtocol.md)
and the runner is `RunCalibratedBeamOn.ps1` (same directory).

## 2. Completed execution and validated accounting

The reviewed runner was invoked once, at commit `77f299b`, with no retry,
replacement, clock reset or source edit.

| Quantity | Value |
|---|---|
| Scientific start / absolute deadline | 2026-11-08T22:01:45.3126158Z / 2026-11-09T02:01:45.3126158Z |
| Exit | 0, at 2026-11-08T23:33:19.9158065Z; elapsed 5494.5 s (1.53 h of the 4 h window) |
| Planned slots | 5380 = 4608 capture + 512 history + 256 schedule + 4 analysis |
| Accepted jobs | 2901 captures (2560 calibration/holdout + 341 fresh), 512 histories, 256 schedules, 4 analysis = 3673 |
| Failed, censored, replaced, retried | 0 each; no realization exceeded 8 arrivals (maximum observed 5 water, 6 oxygen) |
| Unexecuted | 1707 fresh capture slots (2048 reserved, 341 needed). These are unused reservations, not failures |
| Zero deposits | One (water calibration), retained in every denominator |
| Process caps | All 3673 `.meta` files end `reason complete`; no cap notification. Peak job memory 57.7 MiB (cap 512), longest job 19.34 s as reported by the launcher (cap 60), lowest free memory 5.44 GiB (floor 3) |
| Job time | By ledger start and finish: capture 4535 s (mean and median 1.56 s), schedule 53.8 s, history 278.4 s (mean 0.54 s, max 19.50 s), analysis 29.8 s |

Validation, from `06-audit/audit-summary.json` and `local-report.md`:

- 7088 native re-audits passed and none failed: 3673 process, 2901 capture, 512
  history, 2 identity. All 2901 energy ledgers balance.
- The scientific seed manifest holds 14594 unique seeds, none shared with the
  19602 prior inputs (58 registries) inventoried at startup; the six control
  seeds `900000001`–`900000006` are reserved.
- Preservation comparison of the 39663 inventoried original-study files before
  and after the run: 0 mismatches. Frozen-input and prior-input hash checks:
  0 mismatches.
- A copy of the study, re-analysed after logged path relocation, reproduced
  both original summaries, support tables, derived streams and capture ledgers
  byte for byte. The first copied analysis exited 2 with missing, censored and
  duplicate-pair diagnostics; path relocation is the supported explanation,
  and exclusive causation was not established.
- Two independent result reviews found no scientific acceptance blocker. Their
  one report finding (the exclusive attribution of that exit 2) is qualified
  in the report and here. The reviews also checked 51874 observed activation
  identities and times across the 512 histories and 256 paired packet and time
  identities.

"Accepted" means complete, audited and truthfully reported. It does not mean the
precision target was met (section 3), and it does not extend past the inventoried
evidence: this is not an atomic filesystem snapshot or proof that no unrelated
file changed.

## 3. Both 5% calibration targets were missed

| | Water | Oxygen |
|---|---:|---:|
| Calibration captures | 1024 | 1024 |
| Calibration mean (MeV) | 7.2052e-4 | 6.8130e-4 |
| Sample SD / mean | 1.31 | 1.31 |
| **Relative 95% half-width** | **8.05398%** | **8.01033%** |
| Target | at most 5% | at most 5% |
| Maximum single deposit (MeV) | 0.01125 | 0.01873 |
| Holdout captures / relative half-width | 256 / 11.76% | 256 / 12.89% |
| Calibration minus holdout, Welch 95% (MeV) | [-4.99e-05, 1.46e-04] | [-9.80e-05, 1.08e-04] |

The holdout agrees with the calibration within noise, so the miss is a
sampling-precision shortfall, not a sign of a drifting or inconsistent
calibration. Deposits are heavy-tailed (SD about 1.3 times the mean).

The calibration mean sets the arrival intensity and therefore the mean number of
sources and the delivered dose, so its 8% uncertainty reaches every result. The
fixed design was not enlarged after seeing this, and no further sampling is
authorized. For orientation only: at the same SD, a 5% half-width would need
roughly 2.6 times the calibration sample (about 2650 captures per condition,
1024 × (8.05/5)² ≈ 2657 water, ≈ 2628 oxygen). That is arithmetic, not a plan.

## 4. Three kinds of interval

All three are percentile intervals from 2000 replicates (`mt19937_64`,
SplitMix64 domain seeds, linear interpolation), computed in
`CalibrationStatistics.cc`.

| Interval | What is resampled | What it answers | What it must not be used for |
|---|---|---|---|
| **Conditional** | The 128 realizations (their paired differences), unit weights | Sampling variation of the 128 realizations at the deployed, fixed arrival intensity | It leaves out calibration uncertainty |
| **Calibration-only** | The 1024 calibration deposits; the observed 128 pairs are kept and each is reweighted by a Poisson likelihood ratio | How the observed contrast would move if the calibrated intensity had been different | Significance. The pairs are not resampled, so these intervals are narrow and can exclude zero when the conditional one does not (water H2O2: calibration-only [0.176, 0.233], conditional [-0.141, 0.516]) |
| **Combined** | Both at once | Approximate sampling-plus-calibration design sensitivity | Fixed-intensity inference, an unconditional coverage guarantee, or biological equivalence |

The weight is `exp(mu0 - mub) (mub/mu0)^N`, where `mu0` is the nominal Poisson
mean from the frozen calibration mean, `mub` the mean implied by a bootstrap
calibration mean and `N` the realization's arrival count. It is applied once per
pair (both rate arms share it) and is unnormalized. Support gates were checked on every one of the 2001
rows (nominal plus 2000 replicates), with no clipping, no tail renormalization
and no dropped replicate:

| Gate | Requirement | Water | Oxygen |
|---|---|---:|---:|
| Effective sample size | at least 64 | min 124.6 | min 124.2 |
| Largest normalized weight | at most 0.10 | max 0.01206 | max 0.01392 |
| Poisson P(N > 8) | at most 0.001 | max 6.6e-6 | max 1.1e-5 |

Passing the gates is an operational check. Reweighting cannot represent
histories with more than 8 arrivals that were never simulated, so those tail
contributions stay unquantified in every combined interval.

Conditional and combined intervals agree, in both conditions, on which contrasts
exclude zero; the combined ones are slightly wider.

## 5. Results (exploratory)

All contrasts are 1000 minus 100 Gy/s over 128 paired realizations. A **bold**
difference marks a row whose conditional and combined intervals both exclude
zero; no multiplicity adjustment was made, and the species are strongly
correlated, so the number of bold rows is not a count of independent findings.
Several are marginal (a lower bound within about 0.05 of zero). The `species:`
prefix of the report's metric names is dropped; values are rounded to four
significant figures from `06-audit/local-report.md`.

### Water

Mean actual dose 0.002114 Gy; mean arrivals per realization 1.234 (identical at
both rates by pairing).

| Metric | 100 Gy/s | 1000 Gy/s | Difference | Conditional 95% | Calibration-only 95% | Combined 95% |
|---|---:|---:|---:|---|---|---|
| `H2O2^0` | 4.523 | 4.727 | 0.2031 | [-0.1406, 0.5156] | [0.1764, 0.233] | [-0.1413, 0.5194] |
| `H3O^1` | 23.45 | 26.3 | **2.859** | [2.078, 3.664] | [2.605, 3.133] | [2.041, 3.731] |
| `HO_2^-1` | 0 | 0.02344 | 0.02344 | [0, 0.04688] | [0.02131, 0.02581] | [0, 0.05082] |
| `HO_2°^0` | 0.08594 | 0.3203 | **0.2344** | [0.1172, 0.3672] | [0.2126, 0.2575] | [0.1139, 0.3757] |
| `H^0` | 6.133 | 5.555 | **-0.5781** | [-1.047, -0.09375] | [-0.6613, -0.5034] | [-1.066, -0.0966] |
| `H_2^0` | 4.031 | 3.93 | -0.1016 | [-0.3906, 0.1406] | [-0.1067, -0.09515] | [-0.3934, 0.1447] |
| `OH^-1` | 3.68 | 3.398 | -0.2812 | [-0.6797, 0.1172] | [-0.3174, -0.2465] | [-0.6795, 0.1174] |
| `O^-1` | 0.01562 | 0 | -0.01562 | [-0.03906, 0] | [-0.01876, -0.0129] | [-0.04142, 0] |
| `O_2^-1` | 0.5 | 0.1797 | **-0.3203** | [-0.4768, -0.1795] | [-0.355, -0.2882] | [-0.4914, -0.173] |
| `O_2^0` | 0.1016 | 0.1172 | 0.01562 | [-0.02344, 0.05469] | [0.01452, 0.01668] | [-0.02099, 0.05445] |
| `e_aq^-1` | 20.74 | 22.22 | **1.477** | [0.7732, 2.266] | [1.331, 1.63] | [0.7745, 2.276] |
| `°OH^0` | 22.51 | 23.12 | 0.6094 | [-0.09375, 1.406] | [0.5695, 0.6468] | [-0.09795, 1.425] |
| `°O^0` | 0.5781 | 0.5 | -0.07812 | [-0.2188, 0.04688] | [-0.08241, -0.07288] | [-0.2192, 0.04948] |

Five of 13 contrasts exclude zero: `H3O^1`, `HO_2°^0`, `H^0`, `O_2^-1`, `e_aq^-1`.
All-zero rows (omitted): `oxygen_pool_change`, `H2O^-1`, `H2O^0`, `H2O^1`,
`None^0`, `O^0`, `O_3^-1`, `O_3^0`.

### Oxygenated water (21% O2)

Mean actual dose 0.002635 Gy; mean arrivals per realization 1.430.

| Metric | 100 Gy/s | 1000 Gy/s | Difference | Conditional 95% | Calibration-only 95% | Combined 95% |
|---|---:|---:|---:|---|---|---|
| `oxygen_pool_change` | -35.26 | -32.81 | **2.445** | [1.476, 3.555] | [2.177, 2.768] | [1.449, 3.675] |
| `H2O2^0` | 6.414 | 6.438 | 0.02344 | [-0.2344, 0.3047] | [0.01677, 0.03109] | [-0.2405, 0.3011] |
| `H3O^1` | 28.63 | 32.73 | **4.102** | [2.844, 5.657] | [3.62, 4.681] | [2.857, 5.844] |
| `HO_2°^0` | 0.7344 | 4.562 | **3.828** | [2.586, 5.36] | [3.348, 4.403] | [2.574, 5.609] |
| `H^0` | 0.02344 | 0.5078 | **0.4844** | [0.2656, 0.7422] | [0.4302, 0.5493] | [0.2596, 0.7524] |
| `H_2^0` | 4.539 | 4.797 | **0.2578** | [0.05469, 0.4688] | [0.2303, 0.2899] | [0.0547, 0.475] |
| `OH^-1` | 3.914 | 4.32 | 0.4062 | [-0.03125, 0.8828] | [0.3777, 0.4336] | [-0.03481, 0.9034] |
| `O^-1` | 0.007812 | 0.01562 | 0.007812 | [-0.01562, 0.03145] | [0.006727, 0.009018] | [-0.01618, 0.03472] |
| `O_2^-1` | 35.43 | 29.01 | **-6.422** | [-8.813, -4.547] | [-7.351, -5.647] | [-9.31, -4.484] |
| `e_aq^-1` | 0.125 | 2.43 | **2.305** | [1.484, 3.266] | [2.028, 2.643] | [1.461, 3.381] |
| `°OH^0` | 27.65 | 28.68 | **1.031** | [0.04688, 2.211] | [0.8745, 1.23] | [0.05447, 2.295] |
| `°O^0` | 0.5625 | 0.6719 | 0.1094 | [-0.03125, 0.2578] | [0.09762, 0.1222] | [-0.03462, 0.268] |

Eight of 12 contrasts exclude zero: `oxygen_pool_change`, `H3O^1`, `HO_2°^0`, `H^0`,
`H_2^0`, `O_2^-1`, `e_aq^-1`, `°OH^0`. All-zero rows (omitted): `H2O^-1`,
`H2O^0`, `H2O^1`, `HO_2^-1`, `None^0`, `O^0`, `O_2^0`, `O_3^-1`, `O_3^0`.
`actual_dose_Gy` and `source_count` have zero contrast by pairing in both tables.

## 6. Exploratory differences versus unproven inter-track attribution

The tables show how final counts differ between the two delivery protocols. They
do not say why. The two arms differ in more than how close tracks are in time:

1. **Exposure and observation time.** The arms are observed at 20 µs + 2 ps and
   2 µs + 2 ps. Molecules from early arrivals are ten times older at
   observation in the low-rate arm, so decay, scavenging and recombination have
   run for longer there even for a single isolated track.
2. **Arrival spacing.** The same normalized arrival times are stretched tenfold
   in absolute time.
3. **Inter-track reactions.** Only this third mechanism is the one the question
   usually means, and the design does not isolate it.

In oxygenated water the pattern (aqueous electrons 0.125 vs 2.43, H atoms 0.023
vs 0.51, `HO_2°` 0.73 vs 4.6, O2•− 35 vs 29) is the kind an age difference alone
could produce, since 21% O2 scavenges electrons and H atoms. That reading is
untested here.

One direct instrument exists in the retained histories: the `disjoint` counter
in `BeamOnHistory.cc`, which counts bimolecular reactions whose two reactants
both carry a source-ancestry mask and share no source. A post hoc, read-only
scan of all 512 history files (`07-handoff-verification/disjoint_counts.py` in
the evidence root) found **2 such reactions in total**: one in a 4-source water
history and one in a 5-source oxygen history, both at 100 Gy/s; none at
1000 Gy/s. Of the 128 histories per arm, 46 (water) and 52 (oxygen) had at
least two sources. The earlier four-realization fresh control in
[beam-on-irt.md](beam-on-irt.md) likewise saw none.

This points away from direct reactions between molecules of different sources
as the driver of the differences above. It is not proof, because the counter
does not see:

- a reaction in which either reactant has no recorded ancestry;
- the downstream chemistry of a cross-source reaction: its products carry the
  union of both masks, so their later reactions with either parent source's
  molecules are never counted as disjoint;
- bulk-scavenger reactions, which are not two-track events, so the shared O2
  pool can couple tracks without any `disjoint` count (relevant to
  `oxygen_pool_change`).

Ancestry coverage was also not independently audited. Attribution stays unproven
until matched single-track controls are run (see the handoff).

## 7. Remaining limits

### Source count

- Arrival counts are Poisson with nominal means 1.1088 (water) and 1.1726
  (oxygen), computed from the frozen calibration mean. Realized means were 1.234
  (158 arrivals) and 1.430 (183 arrivals).
- Arrivals per realization (post hoc, not predeclared):

  | Arrivals | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | Water | 44 | 38 | 26 | 13 | 6 | 1 | 0 |
  | Oxygen | 30 | 46 | 32 | 12 | 4 | 3 | 1 |

  Empty realizations carry no chemistry and contribute exact zero differences,
  leaving 84 (water) and 98 (oxygen) non-empty realizations.
- **Oxygen shows an arrival-count excess**: 183 observed against 150.1 expected
  (exact two-sided p = 0.0089; chi-square on the histogram p = 0.014). Water is
  compatible with Poisson (158 against 141.9, p = 0.179; chi-square p = 0.203).
  Dispersion (p = 0.081 water, 0.264 oxygen) and normalized arrival times
  (Kolmogorov–Smirnov p = 0.89, 0.24) are consistent with Poisson and uniform.
  Both conditions were examined and nothing was corrected for multiplicity.
- The draws use the frozen mean, so the excess is not calibration error. If the
  generator is correct it is sampling variation already inside the conditional
  intervals. The generator was not re-examined. This is an open question.
- Low and high arms agree on the arrival count in all 128 pairs, in both
  conditions.
- Dose is not fixed. Mean realized dose is 0.002114 Gy (5.7% above the 0.002 Gy
  nominal) in water and 0.002635 Gy (31.7% above) in oxygen. Pairing makes the
  dose identical within a pair, so it does not enter the contrasts, but the
  two conditions are not compared at the same mean dose.

### Geometry

- The regime is a 4 µm cube, 500 keV packets that deposit about 0.7 keV each in
  the cube on average, 0.002 Gy and at most eight sources. Nothing here establishes cost or accuracy at
  a 0.5 mm geometry, doses up to 10 Gy, production streaming, or a mesoscopic
  tail.
- The cube size and its reflecting boundaries set how often molecules from
  different tracks can meet. Other geometries may differ.

### Uncertainty

- Calibration precision is 8%, not 5% (section 3). No comparison is
  precision-qualified.
- The contribution from histories with more than 8 arrivals is not quantified.
  Combined intervals are bounded exploratory approximations, not coverage
  guarantees and not evidence of biological equivalence.
- 128 pairs per condition, with heavy-tailed deposits, give wide conditional
  intervals; some bounds sit at exactly zero.
- The bootstrap streams were not independently regenerated from their domain
  constants. The full molecule payload and every schedule row were not
  independently reconstructed.
- No multiplicity adjustment (section 5).

### Provenance

- The runtime is a private Geant4 11.4.3 build with three experimental patches,
  not the shared install. Matching module paths, the core DLL hash and the
  relevant configured sources do not prove every binary's build origin or
  wholesale physics validity.
- Application timestamps are not authenticated external clocks.
- Preservation checks cover inventoried files, not every unrelated file, and
  are not an atomic snapshot against external writers.
- The cause of the Study05 failure (a Windows `Move-Item` sharing error) is
  understood as a mechanism; the monitoring handle that triggered it is unproven.

## 8. Accepted evidence versus excluded attempts

| Attempt | Evidence (under `.scratch/tests/2026-11-08__beam-on-fresh-source-calibration/`) | Status |
|---|---|---|
| **Study06** | `06-study`, `06-audit`, `06-preflight`, `06-evidence.zip` | **Accepted.** The only source of every number on this page |
| Study06 first preflight, blocked at `7b6b326` | `06-preflight/handoff.md` | Blocked on seed-registry discovery before any launch; repaired in `77f299b`. Not scientific data |
| Study05 | `05-study`, `05-audit` | Excluded. Runner exit 1 after 109 s and 65 water captures. Not in any statistic |
| 05 native controls (seeds `900000001`–`900000006`) | `05-preflight` | Control-only. Re-hashed by Study06 (`06-preflight/control-verification.csv`); not scientific samples |
| Study04 | `04-study`, `04-audit`, `04-preflight` | Excluded. Interrupted after 12 accepted captures. A slash-sensitive replacement in an operator control command wrote its outputs into `03-native-controls`, overwriting earlier control files, which are not immutable originals |
| Mock and synthetic runner tests | `05-ledger-*`, `fresh06-*`, `03-runner-checks*` | Verify the runner. Not scientific evidence |
| Earlier bounded references | [beam-on-irt.md](beam-on-irt.md) | Context only: 64 empirical-bank histories, 16 fresh-control histories, 18 cost controls. Do not pool them with Study06 |

## 9. Integrity of the archive and the report supplement

`06-evidence.zip` (112,282,940 bytes) is the immutable pre-review archive. Its
SHA-256 is `B119CE975F8991EB37158F23D4B4622415ECBDBB7541355BE8A21B1BE580214F`.
It was checked read-only against the existing records during this handoff, and
nothing under `06-*` was modified.

| Check | Result |
|---|---|
| Zip SHA-256 recomputed vs `06-evidence.zip.sha256.txt` | Equal |
| `zipfile.testzip()` (79414 entries, 13 directories) | No bad member |
| Rows of `06-audit/archive-manifest.csv` (79397) present in the zip with matching SHA-256 | 79397 of 79397 |
| Same rows matching the live file | 79396; the exception is `06-audit/local-report.md`, the intended supplement below |
| Zip members absent from the manifest | 4, by design: three `archive-manifest.csv` files and `06-archive-scope.txt` |

**Report supplement.** After the independent reviews, the final paragraph of
`06-audit/local-report.md` was qualified and a review section appended. The
archive keeps the pre-review text; the live file is a separately hashed
supplement:

| | SHA-256 | Bytes |
|---|---|---:|
| Pre-review (in the zip) | `4BA1026A5DC4114E854D9C8DC0C2F32FD98FCF9F343852BAEE8E98F3EF1E78F4` | 13729 |
| Live post-review (matches `06-audit/post-review-report-hash.csv`) | `70A59F58316FFE30FE03B6CCEBB4D55B780B35BAB7582A04739F83A11F482F26` | 15409 |

They differ only in that paragraph and the appended review section; every number
and table is identical. The old wording said the copied-analysis exit 2 was
"solely a logged mixed-separator path-relocation error". The qualified wording is
the one to cite. `06-audit/handoff.md` inside the archive still carries the old
wording and is superseded on that point only.

Edits to tracked plans and docs after the run do not alter the frozen
snapshots in `06-study/input-bytes/`, `06-study/protocol-at-run.md` and
`06-study/new-study-declaration.md`. Do not read the later documentation changes
as run-time hash drift.

The checking scripts and their output, the pre-review report copy and the
arrival, disjoint-reaction and table scripts are in
`07-handoff-verification/` (outside the archive scope; see its `README.md`).

## 10. Superseded guidance

These now point here: the "Execution status" section of
`.claude/plans/beam-on-fresh-source-calibration.md`, the "Proposed study06"
heading of `.claude/plans/beam-on-fresh-source-calibration-new-study.md`, the
closing "READY for independent RESULT review" status in
`.claude/plans/beam-on-fresh-source-calibration-implementation.md`, and the
Ticket04, 05 and fresh-control sections of [beam-on-irt.md](beam-on-irt.md).
Their history is kept; their status is not current.
