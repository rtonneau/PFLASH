# Separated-sources check

This is the result of a bounded check: a combined IRT_syn history with several sources
present at once must match the sum of the same sources run alone. The method and the
tolerance are documented in `include/SeparatedCheck.hh`. The run is driven by
`scripts/RunSeparatedCheck.ps1`, and the comparison is done by the Geant4-free `compare`
executable.

## What is compared

Every run uses the same three precursor packets at the same placements; only the
chemistry seed differs. For each record time `T` and each species:

- `C(T)` is the mean over 16 combined histories (chemistry seeds 101..116).
- `E(T)` is the sum, over the 3 sources `k`, of the mean over 16 single-source histories
  (seeds 1001.., 2001.., 3001..).

All 64 chemistry seeds and the 3 capture seeds are distinct, and the script enforces this.

A single-source history activates its source at the source's scheduled time `t_k`, with
the same offset as in the combined run. It records on the record grid of the full
schedule, so its rows line up with the combined history row by row, and its count is
exactly 0 before its activation (`compare` refuses anything else). `E(T)` is therefore
the sum over sources at molecular age `T - t_k`, without interpolation.

The age reported for a row is the time since the latest activation. Here it runs from
1 ns to 2 ms: the schedule activates sources at 0, 0.5 ms and 1 ms, and the endpoint is
3 ms.

## Inputs of the recorded run

- **Schedule:** `config/deterministic-schedule.txt`, 3 sources about 100 um apart
  (SHA-256 `7b83313b...2f471852`).
- **Packets:** 3 packets of 10 keV e-, captured with `macros/capture.mac` from first seed 1.
  They hold 1061, 1079 and 1097 molecules. The energy residual is at most 4.7e-10 eV.
- **Histories:** 16 combined and 16 x 3 single, so 64 in total. The endpoint is 3e6 ns,
  with 97 record times and 16 species. The domain is unbounded pure water with no
  scavenger.
- **Caps:**
  - At most 6 histories run in parallel.
  - A history may run for at most 30 min; beyond that its process tree is killed and
    the run is marked as timed out.
  - The script refuses a non-empty output directory.
- **Records:** the full inputs, seeds and runtimes are in `run-info.json` and `runs.csv`,
  written next to the results.

## Tolerance

The pass rule is the one from the plan:

`|C - E| <= 4 * sqrt(SE_C^2 + SE_E^2) + 0.05 * E`

- `SE_C` is the sample standard deviation of the 16 combined counts divided by
  sqrt(16), with denominator n - 1.
- `SE_E` is computed the same way from the 16 per-seed sums of the three single-source
  counts.

`compare --floor <x>` can add an absolute floor in molecules. The script uses 0, so the
exit status follows exactly the rule above. Each cell in `cells.csv` records which term
decided it (`basis`).

## Result

Run of 2026-11-09 (`separated-check-v2`), exit status 0:

```
SEPARATED_CHECK_PASS cells=1552 failed=0 timeCount=97 species=16 combinedSeeds=16 singleSeedsPerSource=16 sources=3
```

All 1552 cells (97 times x 16 species) passed, each one on the statistical term alone.
Not every cell is a real test, though:

- **223 cells cannot fail:** both ensembles are 0 there. These are the `O^0`
  configuration (never produced; the oxygen atom appears as `°O^0`), `O_3^-1` (never
  produced in this run) and `O_3^0` at 29 times.
- **206 more cells are weak:** they are nonzero but have C + E < 2 molecules.
- **About 1120 cells carry real statistical weight.**

The largest |C - E| / tolerance is 0.63 (`°O^0` at 10 ns: 1.6 vs 2.8 molecules).

Final time (3 ms, age 2 ms after the last activation), C against E:

| species | C(final) | E(final) | SE | ratio |
|---|---|---|---|---|
| H2O2 | 245.1 | 253.4 | 5.1 | 0.25 |
| H3O+ | 704.4 | 715.2 | 4.3 | 0.20 |
| e_aq- | 435.7 | 442.3 | 5.3 | 0.15 |
| OH radical | 476.4 | 467.1 | 7.2 | 0.18 |
| H | 228.6 | 230.8 | 5.2 | 0.07 |
| H2 | 187.6 | 188.9 | 3.9 | 0.05 |
| OH- | 256.2 | 258.8 | 4.9 | 0.08 |
| O- | 15.4 | 16.6 | 1.6 | 0.16 |
| HO2 | 8.6 | 8.7 | 1.0 | 0.03 |

`compare/summary.txt` has the full per-species and per-time table: all 97 record times,
including 1 ns, 1 us and 1 ms, and the ages after each later activation. `cells.csv` has
every cell.

**Runtime:** 64 histories took 68 s wall (sum 370 s, longest 18 s, 6 in parallel). The
whole script took 72 s on a 20-thread machine. An earlier run with capture seed 1001
and a 0.5-molecule floor also passed (1552/1552, worst ratio 0.54).

### Controls in the same script

- **Margin:** with only the 4-sigma statistical term (no relative term) the check also
  passes (`compare-statistical-only/`).
- **Negative controls:** the same combined histories are compared with the expectation
  missing one source. Each comparison must fail, and the script exits 1 if one passes,
  or if `compare` exits 1 without printing `SEPARATED_CHECK_FAIL`.
  - Without the last source (1 ms): 264 of 1552 cells fail.
  - Without the middle source (0.5 ms): 484 of 1552 cells fail. This covers the window
    from 0.5 ms to 1 ms as well.

  The output is `NEGATIVE_CONTROL_OK`.

## Where the output is

Scratch results (not committed): `.scratch/tests/2026-11-09__reusable-poisson-track-irt-example/separated-check-v2/`.
Look in `compare/summary.txt`, `compare/cells.csv`, `compare/summary.json`, `runs.csv`
and `run-info.json`. Cited copies are in
`.scratch/tests/2026-11-09__reusable-poisson-track-irt-example/evidence/04-*`.

Re-run:

```powershell
pwsh examples/poisson-track-irt/scripts/RunSeparatedCheck.ps1 -BuildDir <build> -OutDir <empty dir>
```

## Limitations

- **Spatial regime:** the three sources are about 100 um apart, and the run uses no
  reflecting box (the default, `halfBoxUm` unset). The check tests the independence
  assumption in that regime, not tracks that overlap in space.
- **False failures:**
  - With 16 seeds, the 4-sigma test has about 30 degrees of freedom. The probability
    that a single cell fails by chance is about 4e-4 (Student t), before the 5%
    relative term reduces it further.
  - Over roughly 1100 informative cells, which are strongly correlated in time, a
    chance failure in a run is therefore not negligible.
  - Re-run a failure with other seeds before reading it as a defect.
- **Packets:** one fixed set of 3 packets is used throughout. The check tests the
  chemistry bookkeeping (shared history, timed injection), not the spread between
  packets.
- **Chemistry:** pure water, IRT_syn, default chemistry only, with no scavenger and no
  mesoscopic stage.
- **Age offset:** a source's age is measured from its schedule time. Its molecules carry
  the 1 ps capture age, which the age ignores.
- **Rare species:** `°O`, `O2`, `O2-`, `HO2-` and `O3` stay at 0 to a few molecules. With
  16 seeds they are only loosely constrained.
- **Untested failure paths:** the timeout and process-kill path of the script has not
  been exercised.
