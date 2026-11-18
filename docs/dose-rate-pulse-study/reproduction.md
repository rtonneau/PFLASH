# Reproduction: code, builds, commands, data

## Geant4 install used

All dose-rate runs (2026-11-15 and 2026-11-16) use a **private patched** Geant4 11.4.3 install,
not the shared `${G4_ROOT}` (which must stay unmodified):

- location: `${DEV_DIR}/GEANT4/geant4-v11.4.3-mf-ti-install` (patch set `mf-ti`, marked with
  `g4-build-info.json`; also the default `-Geant4Bin` of `studies/run_study.ps1`). The runs of
  2026-11-15 and 2026-11-16 used it at its earlier location,
  `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install`; it was moved unchanged on
  2026-11-18 (same files and DLL hashes);
- content: pristine 11.4.3 source (`${DEV_DIR}/GEANT4/geant4-v11.4.3-source`) plus three patches
  from `patches/geant4/11.4.3/`:
  - `molecular-finder/coincident-neighbors.patch` (issue 1);
  - `timed-injection/source-contacts-experimental.patch` (issue 2);
  - `timed-injection/meso-boundaries-experimental.patch` (issue 3, plus the unused
    mesoscopic changes of issue 7).

  `meso-bulk-instrumentation-experimental.patch` is **not** in it.
- build and verification procedure (nine patch-owned files, DLL hash): `docs/dose-rate/beam-on-irt.md`,
  "Reproduction" section. The patched code is compiled into `G4processes_core.dll`. Copying
  headers is not enough.

To rebuild it elsewhere: copy the pristine source to a private directory, `git -c
core.autocrlf=false apply` the three patches, then configure/build/install with the project's
MSVC x64 + Ninja + vcpkg settings (`.claude/geant4-instructions.md`).

## Code of this session (branch `feat/per-track-reactions`, uncommitted)

| File | Change |
|---|---|
| `examples/dose-rate-study/include/ReactionLog.hh`, `src/ReactionLog.cc` | new: Geant4-free per-track reaction log (see [methods.md](methods.md)) |
| `examples/dose-rate-study/test/ReactionLogTest.cc` | new: unit test |
| `examples/dose-rate-study/include/ActivationObserver.hh`, `src/ActivationObserver.cc` | configure source ID ranges, learn live tracks at each activation, record reactions in `UserReactionAction`, call `ObserveStep` after each time step, skip `H2O^0`/`H2O^+` |
| `examples/dose-rate-study/src/HistoryMain.cc` | write `reactions.csv`, ledger `reactions` block, counts in `HISTORY_DONE` |
| `examples/dose-rate-study/CMakeLists.txt` | `ReactionLog.cc` in `drsCore`, `ReactionLogTest` |
| `.claude/plans/2026-11-16-per-track-reactions.md` | plan and status |

The pulse-by-pulse age grid (`ageGridPointsPerDecade`, `species_age.csv`) was already in `main`
(merged in PR #6).

Before committing:
- The explorer rule in `CLAUDE.md` (list new files in `docs/explorer/architecture.json`)
  covers the main application's `src/` and `header/` only. `architecture.json` has no
  `examples/` entry, so the new example files need none.
- Reread the plan file.

## Build and test

```
cmake -S examples/dose-rate-study -B examples/dose-rate-study/build-ninja -G Ninja `
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=<private patched install>/lib/cmake/Geant4
cmake --build examples/dose-rate-study/build-ninja
ctest --test-dir examples/dose-rate-study/build-ninja --output-on-failure      # 15/15 pass
```

- In this example, `build-ninja/` is **RelWithDebInfo**, not Debug as in the main project. A
  Debug build crashes against the Release install (STL layouts differ).
- Run from the MSVC x64 environment, with the patched install's `bin/` first on `PATH`.
  `run_study.ps1` does that.

## Runs

From `${STUDIES_ROOT}/dnachem-min/2026-11-16__pulse-by-pulse/` (the study JSONs are there):

```
python examples/dose-rate-study/studies/make_configs.py --library ../2026-11-15__dose-rate-study/spike/library \
       --study study_pulse.json --out plan_pulse_rx
pwsh examples/dose-rate-study/studies/run_study.ps1 -Plan plan_pulse_rx/plan.json -Parallel 9
# same with study_pulse_lo.json -> plan_pulse_lo_rx (-Parallel 3)
```

These runs predate the `/study` skill (ADR 0012). A rerun whose results are used adds
`-Study <study> [-RunSet <name>]`, so each history gets a provenance stamp under
`${STUDIES_ROOT}` (`examples/dose-rate-study/README.md`, "Study tooling").

Per run, `out/<id>/` holds:
- `activations.csv`: one row per track, with arrival and activation time and deposit;
- `ledger.json`: checks, counts, `reactions` block;
- `species.csv`: absolute time grid;
- `species_age.csv`: per-track age grid;
- `reactions.csv`: rx runs only.

`logs/<id>.log` holds the console output. `run_log.csv` has one row per run (wall time, peak
memory, status).

## Analysis and figures

Scripts in `.scratch/deck-20261117/` (git-ignored; Python from the GEANT4 conda env; figure
scripts use the `ppt_figures` skill's `pptfig` module). The data root is hard-coded as `BASE` and
the figure output as `OUT`/`PPT_RESSOURCES` at the top of each script. Edit them on another
machine. A copy of the `*.py` scripts as they were on 2026-11-18 is in the Study, under
`analysis/deck-20261117/input/`, with their SHA-256 in `analysis/deck-20261117/stamp.json`.

| Script | Input → output |
|---|---|
| `pulse_analysis.py` | `species_age.csv`, `activations.csv` → `pertrack.csv` |
| `rx_analysis.py [plan dirs]` | `reactions.csv`, `activations.csv`, `ledger.json` → `pertrack_rx.csv` (≈ 699k rows), `tracks_rx.csv` (5496 tracks), `taubins_rx.csv` |
| `rx_tables.py` | helpers (`load`, `gcum`, `at`) used by `fig_rx.py` |
| `fig_rx.py` | reaction figures `rx_*`, budgets `rx_budget_H2O2.csv`, `rx_budget_H2.csv`. Env: `TAUMAX` (x-axis end, ns), `TAU_PARTNER` (partner table age), `TAU_BUDGET` (budget age). The deck figures used `1e5` for all three. |
| `fig_pertrack.py` | per-track G figures `pt_*` (by rate, by dose delivered) |
| `fig_rate.py`, `fig_rate2.py`, `fig_h2o2.py`, `fig_sawtooth.py` | box G(t) / dose-rate figures `dr_*` |
| `fig_algos.py` | algorithm schematics `algo_*` |
| `movie_slides.py` | movie frames `dose_rate_movies/` from the 2026-11-15 snapshot runs |

Data outputs (`pertrack*.csv`, `tracks_rx.csv`, `taubins_rx.csv`, `rx_budget_*.csv`) are written
to `${STUDIES_ROOT}/dnachem-min/2026-11-16__pulse-by-pulse/`.

```
cd .scratch/deck-20261117
python pulse_analysis.py
python rx_analysis.py
TAUMAX=1e5 TAU_PARTNER=1e5 TAU_BUDGET=1e5 python fig_rx.py
python fig_pertrack.py
```

(Bash; set `PYTHONIOENCODING=utf-8` when printing to the console.)

## Where the data live

The run data were moved out of `.scratch/` on 2026-11-18 into the Studies
`2026-11-15__dose-rate-study` and `2026-11-16__pulse-by-pulse` under `${STUDIES_ROOT}/dnachem-min/`
(outside git, listed in [docs/studies/INDEX.md](../studies/INDEX.md)). Each Study holds a
`study.json` and reconstructed provenance stamps (`runs/<set>/stamp.json`). To rerun, you need:
- `${STUDIES_ROOT}/dnachem-min/2026-11-15__dose-rate-study/spike/library` (packet library);
- the patched install (`${DEV_DIR}/GEANT4/geant4-v11.4.3-mf-ti-install`), or rebuild it as
  described above.
