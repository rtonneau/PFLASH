# Continuous-beam dose-rate G-value study: tooling

Scripts that turn a captured packet library (100 keV `e-` track segments crossing a
5 um water box, `macros/capture-100keV-entry-face.mac`) into Poisson-arrival histories at
several dose rates, run them with `history` and compute G-values. Pipeline:

```
make_schedules.py -> run_study.ps1 -> analyze.py -> plot.py
```

Python: `C:\Users\rtonneau\AppData\Local\miniconda3\envs\GEANT4\python.exe` (numpy, matplotlib).
Tests: `python -m unittest discover -s examples/poisson-track-irt/studies/continuous-beam -v`.

## 1. make_schedules.py

```
python make_schedules.py --library <library dir> --study study.json --out <new plan dir> [--output-root <dir>]
```

`study.json` (only `rates`, `realizations`, `sourceSeed` are required):

```json
{
  "rates": [0.1, 1, 10, 100, 1000],
  "realizations": {"0.1": 400, "1": 200, "10": 100, "100": 50, "1000": 20},
  "sourceSeed": 12345,
  "boxSideUm": 5.0, "windowNs": 1e6, "endpointNs": 1e6,
  "gridMinAgeNs": 1.0, "gridPointsPerDecade": 3, "chemistrySeedBase": 1000
}
```

Defaults are those shown (`gridPointsPerDecade` 3 because there is one record grid per
arrival). `realizations` is keyed by the rate (compared as numbers).

Method. Box mass `m = (boxSideUm)^3 * 1 g/cm3` (1.25e-13 kg for 5 um). Mean arrival rate
`lambda = Ddot * m / mean(E_dep)` with `mean(E_dep)` the mean of `energyDepositEv` over the whole
`library.csv` (the library mean, not the drawn packets). Capture such a library with
`capture --max-empty M`: some primaries cross the 5 um box without making a precursor molecule.
Those events have no packet, so they are not counted in this mean. Their deposits are listed in
`capture.json` (`skippedEmptyEvents`); check that they are negligible. Per realization: a Poisson count with mean
`lambda * window`, then that many arrival times uniform on `[0, window)`, sorted
(`numpy.random.Generator(PCG64(sourceSeed))`, rates in order, realizations in order). Empty draws
are not run; they are counted per rate in `plan.json` (`emptyRealizations`). Non-empty realizations
take consecutive packet slices, disjoint across all realizations of all rates; the script fails if
the library is too small, naming the packet count needed. Chemistry seeds are
`chemistrySeedBase + k`, with `k` counting all realizations (empty ones too), so they are distinct
and stable.

Packet offsets. Packet molecule positions are stored relative to the primary's start point, so the
schedule offset of each packet is its `primaryOrigin_nm` from the library's `capture.json` (nm
converted to um), which puts the track back at its random point of the -z face. An offset of
`0 0 0` would place molecules up to 5 um beyond the box and `history` refuses it
("lies outside the reflecting box"). `halfBoxUm = boxSide/2` (2.5), endpoint 1e6 ns.

Output in `--out`: `plan.json`, `runs/<id>/schedule.txt`, `runs/<id>/history.cfg`
(`<id>` = `r<rate>_<realization>`). `plan.json` has the study parameters, `rates`, `realizations`,
`emptyRealizations`, `meanArrivalRateHz`, `boxMass_kg`, `outputRoot` and `runs[]` (`id`,
`rate_Gy_s`, `realization`, `chemistrySeed`, `nPackets`, `packetStart`, `config`, `schedule`,
`output`); paths are relative to `plan.json`.

## 2. run_study.ps1

```powershell
pwsh run_study.ps1 -Plan <plan dir>\plan.json [-DryRun] [-Limit N] [-Parallel N] [-History <history.exe>] [-Geant4Bin <patched install bin>] [-LogCsv <csv>]
```

Defaults: `history.exe` from `build-patched`, the patched install's `bin` from
`${DEV_DIR}/GEANT4/geant4-v11.4.3-mf-ti-install/bin` (patch set `mf-ti`) put first on `PATH`;
`GEANT4_DATA_DIR` must be set. Run it from PowerShell, not Git Bash. A run is skipped when its
output holds a `ledger.json` with `checks.allPassed` true; an incomplete output directory (under
the plan's output root only) is deleted and rerun. Each run appends to `run_log.csv` (next to
`plan.json`): `id,startUtc,wallTime_s,exitCode,status,historyDone` (the `HISTORY_DONE` line). It
runs `-Parallel N` histories at once (separate single-threaded processes, longest first; Geant4 MT cannot split one history, whose packets share one event) and exits 1 at the end when any run exited non-zero or failed its checks. Console output of each
run goes to `<outputRoot>/logs/<id>.log`. `-DryRun` prints the commands and changes nothing.

## 3. analyze.py

```
python analyze.py --plan <plan dir>/plan.json [--out <dir>] [--allow-partial]
```

Species: `e_aq^-1`, `°OH^0`, `H^0`, `H_2^0`, `H2O2^0`, `H3O^1`, `OH^-1` (files read as UTF-8).
For each rate, on the anchor-0 grid (`gridMinAgeNs * 10^(k/ppd)` below the endpoint, plus the
endpoint; only times present in every realization of the rate are kept, since a grid time within 5 ps
of an arrival is dropped by `history`):
`G_s(t) = 100 * sum_r N_rs(t) / sum_r E_r(t)` (molecules/100 eV), where `E_r(t)` is the summed
`energyDepositEv` of the sources with `expectedActivationTimeNs <= t`. SE by the delta method over
the n non-empty realizations: `R = sum y / sum x`,
`SE = 100 * sqrt(sum (y_r - R x_r)^2 / (n (n-1))) / mean(x)`; NaN when n < 2. G and SE are NaN where
`sum x = 0` (before any activation). A run whose ledger has `allPassed` false is an error.

- `gvalues_time.csv`: `rate_Gy_s,time_ns,species,G_per_100eV,SE,n_realizations`.
- `gvalues_1ms.csv` (value at the endpoint): `rate_Gy_s,species,G_per_100eV,SE,n_realizations,n_empty,`
  `mean_tracks,mean_dose_Gy,delta_vs_ref,delta_SE`. `mean_tracks` and `mean_dose_Gy` are means over
  all realizations of the rate, empty ones counted as zero (dose check: ~`Ddot * window`).
  `delta_vs_ref = G(rate) - G(lowest rate)`; `delta_SE` combines the two SEs in quadrature (NaN for the
  reference row itself, whose `delta_vs_ref` is 0).

## 4. plot.py

```
python plot.py --data <dir with gvalues_*.csv> [--out <dir>] [--dpi 150] [--species ...] [--ncols 4]
```

`g_vs_rate.png` (G at the endpoint vs dose rate, log x, error bars, one panel per species) and
`g_vs_time.png` (G(t), one line per rate, log t). Plain styling; the functions `plot_g_vs_rate` and
`plot_g_vs_time` take size, dpi and columns for later restyling.
