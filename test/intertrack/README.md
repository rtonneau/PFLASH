# Matched intertrack tools

This standalone C++20 project uses only the standard library. Frozen Study06
probes are read and linked as reusable pure logic; their files are unchanged.
Preparation performs no chemistry, capture or arrival sampling. Native controls
and scientific histories start only through the separate execution command.

Build in an MSVC x64 developer shell:

```powershell
cmake -S test/intertrack -B .scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build .scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/build
ctest --test-dir .scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/build --output-on-failure --no-tests=error
```

From the repository root, freeze a fresh evidence directory:

```powershell
$build = '.scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/build'
$out = '.scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/prepared-study'
pwsh -NoProfile -File test/intertrack/PrepareMatchedStudy.ps1 -BuildDir $build -OutDir $out
```

After independent code/scientific review, execute the frozen declaration once:

```powershell
pwsh -NoProfile -File test/intertrack/RunMatchedStudy.ps1 -OutDir $out
```

The complete job/stream/input/output ownership manifest is hashed before launch.
Preparation checks original evidence against the immutable archive inventory,
current reusable inputs against Study06 snapshots, and shared loaded modules
against the private runtime. It reserves all earlier seed evidence and every
Study06 slot, including unused captures. Changing the tools after preparation
requires a new preparation directory. No existing attempt may be resumed or
replaced. All initial pools and every species map are preserved. Fixed schedules
use original manifest paths and one unchanged row (original packet index); local
native source zero maps to external condition/realization/instance/rate.

There are 682 scientific jobs and 24 controls. For each condition/rate the six
controls are: empty; original singleton with original chemistry seed (same-seed
harness equality against the reused history); singleton with a fresh chemistry
seed; a late arrival at exposure minus 0.5 ps; an identical late repeat; a two-source
subset for activation/identity accounting. Late controls activate at exposure plus
0.5 ps, before the unchanged endpoint exposure plus 2 ps. These controls use
existing packets only. The original singleton and late-repeat cases intentionally
reuse chemistry seeds solely for harness equality; every control still reserves
its own unused slot in streams.tsv. Scientific chemistry seeds are unique and
independent across packets and rates. Analysis uses four separately reserved seeds.
No control accepts/rejects an independent stochastic difference by significance.

All 24 controls must pass before science. One persistent exclusive execution
claim and a monotonic two-hour deadline cover controls and science together.
Serial native jobs get min(60 seconds, remaining budget), 512 MiB committed-private
memory and the launcher's 3 GiB free-memory floor. Persistence time is charged.
Success requires child metadata, marker, loaded modules, and full native ledgers;
a zero launcher exit alone is insufficient. First failure stops further launches;
all 706 planned outcomes remain recorded, with no automatic retries. Failed,
censored or unexecuted jobs withhold chemistry inference for BOTH conditions.

Analysis is run by the bounded script after execution. To reproduce analysis,
use a new evidence copy, preserve the frozen source references and document path
relocations; do not rerun it in an existing report directory. Direct command:
`MatchedStudy.exe analyze <new-evidence-copy>`.

The primary keys are `species:e_aq^-1` in water and oxygen and `bulk:O_2^0`
in oxygen; each has C_100, C_1000, and D = C_1000 - C_100 (nine estimates).
All recorded bulk components use sum(final-initial), never sum(final) minus one
background. Tracked species and bulk species use separate namespaces. Empty
realizations are zero. The observed species union includes both arms and rates.
The independent unit is the complete paired realization. 20000 replacement draws
keep both rates and arms together. Percentiles interpolate at p*(B-1); ordinary
95% and nine-primary Bonferroni intervals use .025/.975 and .05/18,1-.05/18.
Sparse (<10 nonzero units) and degenerate intervals flag inability to bound unseen
rare events. The 0.5 molecule half-width target is reporting only.

Calibration sensitivity resamples the 1024 calibration bank and all 128 units,
uses unnormalized Poisson likelihood ratios once per complete contrast, and checks
ESS>=64, max normalized weight<=.10, P(N>8)<=.001 on original units before
multiplicities for every replicate. Any failed gate suppresses all sensitivity
intervals for that condition; no clipping or successful-replicate selection.
The unquantified overflow tail remains. These are approximate design sensitivity,
not unconditional coverage. S-I is total inter-packet coupling in expectation;
individual differences also include independent chemistry noise. Ancestry counts
are exploratory and cannot partition direct versus depletion effects.
