# dose-rate-study

Standalone Geant4-DNA example for the dose-rate study of a 4 um water box
(ADR 0011, `docs/adr/0011-reflecting-box-for-dose-rate-study.md`). It is copied
from `examples/poisson-track-irt` (packet format, capture, history, source
injection, reflecting box) and from the dnachem-min `PureWater` Chemistry
(`PureWaterReactions.cc`, `ChemistryTypes.hh`). It does not link or include
either; a change there does not reach this example.

Status: ticket 02. `capture` is complete. `history` runs one bounded history,
including the doseCutoff mode (constant dose rate up to a set dose).

## Physics and chemistry

- `G4EmDNAPhysics_option2`, Serial run manager.
- `PureWaterChemistryList`: the molecules and water dissociation of
  `G4ChemDissociationChannels_option1`, the copied PureWater reaction network,
  and the pH 7 acid-base buffer (`H3Op(B)`, `OHm(B)`, as per-molecule
  `G4DNAScavengerProcess` with a `G4DNAScavengerMaterial`). No O2.
- IRT_syn time-step model only, no mesoscopic stage.
- The bulk buffer has its own reservoir (500 um half side, the dnachem-min
  default), not the 4 um box. Geant4 rounds the bulk molecule count down to
  `floor(N_A c V)` and computes the propensity from it. In a (4 um)^3 box,
  pH 7 gives 3 H3O+ instead of 3.85, so the buffer rates would be 22 % low.
  Bulk species are never consumed, so the reservoir sets only the rates.

## capture

Each event shoots one 500 keV e- along +z. The primary starts 1 nm inside the
-z face of a 20 um water world (half side 10 um), at a uniform random (x, y)
over the face of the 4 um capture box (half side 2 um) centred in it.

Each primary becomes one `PrecursorPacket`, holding:

- the molecules strictly inside the box at 1 ps;
- the energy deposited inside the box, whichever particle deposits it, so
  delta rays born upstream count;
- `escapedEnergy`, the rest of the primary energy (deposited outside the box
  or carried out of the world).

Positions are relative to the box centre. Some primaries launched near the box
edge scatter laterally upstream and miss the box (no molecule, no deposit).
`--max-empty N` lets capture skip up to N such events; they are listed in
`capture.json` (`skippedEmptyEvents`).

```
capture --macro macros/capture.mac --output <dir> --count N --first-seed S [--max-empty M]
capture-check <dir> [--expect-count N] [--box-half-side-nm 2000]
```

The macro commands are `/drs/capture/boxHalfSide` and
`/drs/capture/worldHalfSide` (PreInit), plus `/gun/particle` and
`/gun/energy`. capture overrides the gun position and direction.
`capture.json` records `meanBoxDeposit_eV`, plus the deposit outside the box,
the world escape and the dropped molecules, for every packet and in total.

## history

`history --config <file> --output <dir>` takes the config keys of
`include/HistoryConfig.hh`. Set `halfBoxUm = 2` for the reflecting 4 um box.

### doseCutoff

`mode = doseCutoff` irradiates the box at a constant dose rate until it has
received a set dose, all in one IRT_syn history:

```
mode = doseCutoff
libraryDir = <capture library>
halfBoxUm = 2              # must equal the library's capture box
worldHalfSideUm = 3
chemistrySeed = 21
sourceSeed = 7             # arrival sampler
doseRateGyPerS = 1e4
doseGy = 1                 # default 1
steadyStateWindow = 50     # default 50; 0 = no steady-state stop
steadyStateTolerance = 0.05  # default 0.05
```

Optional keys: `firstPacket`, `maxArrivals`, `steadyStateMinMean` and
`ageGridPointsPerDecade` (default 5).

- Box mass: m = rho (2 h)^3 = 6.4e-14 kg for the 4 um box, so 1 Gy is
  3.99e5 eV, about 600 tracks of 500 keV e-.
- Rates: the launch rate is `Ddot m / ebar`. Here ebar is the mean box
  deposit per launched primary, and the empty crossings skipped by capture
  count as 0 eV. Only primaries that hit the box are scheduled, as packets at
  `hitFraction` times the launch rate (thinning). The ledger records ebar, the
  hit fraction and both rates.
- Schedule: the first arrival is at t = 0, then exponential waits, using
  library packets in order from `firstPacket`. Scheduling stops at the first
  arrival whose cumulative box deposit reaches the target dose, so the
  overshoot is less than one track. A source activates 1 ps after its arrival
  (the capture time).
- Beam-off: 1 ps after the last activation (G4Scheduler never processes a
  delayed list due exactly at the end time). It is also the chemistry end;
  there is no post-beam relaxation.
- Grids: a common absolute log grid from 1 ps at 10 points per decade, the
  same for every dose rate, truncated below beam-off, with beam-off appended.
  There is also one age grid per arrival, from 1 ps after it, up to the next
  arrival. Counter times within 1 fs are merged, and E(t) follows the same
  rule.
- Steady-state stop: there is one sample per arrival after the first (live
  counts just before the arrival's source joins). The run stops when every
  species' mean over the last `steadyStateWindow` samples is within
  `steadyStateTolerance` of the previous window's mean (species whose two
  means are both at most `steadyStateMinMean` are skipped). The history then
  ends early, and the later common-grid rows hold the last window's means,
  with `extrapolated = 1`.

Outputs:

- `species.csv`: one row per common-grid time and species, with columns
  `timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated`.
  G by cumulative dose divides by E(t), and G by final dose divides by the
  final deposit.
- `species_age.csv`: the same quantities on the age grids, with
  `arrivalIndex` and `ageNs` added. Only measured points are written.
- `activations.csv`: one row per source, with the expected and observed
  activation times.
- `ledger.json` (format version 2): `rates`, `dose`, `grids`, `steadyState`,
  and checks for activation, end and energy (target reached, overshoot below
  one track, E(beam-off) and the injected deposit both equal to the final
  deposit). `history` exits with status 1 when any check fails.

### Snapshots (doseCutoff, optional)

`snapshots = true` (off by default; `snapshotsPerDecade`, default 3, at most
10) writes `snapshots.h5`: the position (float32, nm) and species id (uint8) of
every live molecule at the common-grid times closest to 3 per decade, up to
beam-off. The species-name table is stored once (`/species`), each frame is a
group `/frames/frameNNNNNN` with a `time_ns` attribute, and the datasets are
chunked with shuffle and gzip when the HDF5 build has the zlib filter
(`ledger.json` `snapshots.compressed` says which). Water tracks are not
stored, as in the species tables.

- HDF5 is optional (`-DDRS_WITH_HDF5=AUTO|ON|OFF`, default AUTO). Without it
  `snapshots = true` is a config error (exit status 2) and no file is written.
- Frame times are the clock times of the frames: a snapshot time is taken at the
  first step end at or after it, and times closer than a few picoseconds to
  another share one frame (`ledger.json` `snapshots.mergedIntoAnotherFrame`).
  The history is stepped to each snapshot time, so with snapshots on its random
  stream differs from a run without them: the results agree statistically, not
  bit for bit.
- `scripts/snapshots.py` reads the file (`snapshots.read(path)` gives the species
  names and the frames), `scripts/movie.py snapshots.h5 --out movie.gif` makes a
  3D scatter animation coloured by species (mp4 when ffmpeg is installed and
  the output is `.mp4`; `--species`, `--max-points`, `--box-half-side-nm`). Both
  need numpy, h5py and matplotlib (Pillow for GIFs).
- `snapshots-check` (built with HDF5) validates a file against `species.csv`;
  the `HistorySnapshots` ctest runs it.

## Build and test

Build against the patched Geant4 11.4.3 install, and pass its path on the
command line only. With MSVC, use the same build type as that install
(RelWithDebInfo against the Release install). A Debug build crashes at the
first `G4String` passed to Geant4, because the debug and release STL layouts
differ.

```
cmake -S examples/dose-rate-study -B examples/dose-rate-study/build-ninja -G Ninja `
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=<patched install>/lib/cmake/Geant4
cmake --build examples/dose-rate-study/build-ninja
ctest --test-dir examples/dose-rate-study/build-ninja --output-on-failure
```

CMake checks at configure time whether the kernel has
`G4DNAIndependentReactionTimeStepper::RegisterSourceActivation`, from
`source-contacts-experimental.patch` (`DRS_HAS_SOURCE_ACTIVATION`). If the kernel
lacks it, for example the stock install or a leave-one-out kernel of
`test/dose-rate-patches`, `history` still builds. It then injects sources as plain
delayed tracks and logs this once. `ledger.json` records the choice as
`sourceActivation` (`true` or `false`).

The tests:

- Unit tests of the Geant4-free core.
- `CaptureSmoke`: 20 packets. It checks the count, that every molecule is
  inside the box and that the mean box deposit is positive, and prints the
  mean box deposit.
- `HistorySmoke`: two packets of that library in the 2 um reflecting half box,
  to 100 ns.
- `HistoryDoseCutoff` and `HistoryDoseSteadyState`: doseCutoff at 1e4 Gy/s.
  The 20-packet library holds only about 0.037 Gy, so these stop at 0.005 Gy
  and 0.02 Gy. The second uses 2-sample windows and a loose tolerance so that
  the steady-state stop fires. Each checks the outputs, the activation checks
  and the energy checks.

## Study tooling (`studies/`)

Python: the GEANT4 conda env (numpy, matplotlib). Tests:
`python -m unittest discover -s examples/dose-rate-study/studies -v`.

```
python studies/merge_libraries.py --out <lib> <capture part dir> ...     # parallel captures, distinct seeds
python studies/make_configs.py --library <lib> --study studies/study.json --out <plan dir> [--realizations 0.1=4 ...] [--snapshots]
pwsh studies/run_study.ps1 -Plan <plan dir>/plan.json -Parallel N [-WallTimeCapS S] [-Study <study> [-RunSet <name>]]
python studies/analyze.py --plan <plan dir>/plan.json [--plan <other plan.json> ...] [--out <dir>] [--runs-dir <run set dir> ...] [--allow-partial]
python studies/plot.py --data <plan dir> [--norm cum|final]
python studies/spike_report.py <dir holding plan dirs>
```

- `study.json`: rates, realization counts, dose (1 Gy), steady-state window
  (50) and tolerance (5 %), seed bases, progress interval, wall-time cap.
- `make_configs.py`: one doseCutoff config per rate and realization, distinct
  chemistry and source seeds; each run starts at a random library offset that
  still holds the target dose plus one track.
- `run_study.ps1`: resumable, parallel; samples the peak working set; kills a
  run beyond the cap (status `truncated`, its last `HISTORY_PROGRESS` line is
  kept). Log: `run_log.csv` next to `plan.json`. With `-Study`, each history is a
  Study run (`study.ps1 run <study> <runSet> --id <run id>`, seeds and rate recorded,
  see `docs/agents/study.md`): its output goes to
  `${STUDIES_ROOT}/dnachem-min/<study>/runs/<runSet>/<id>/output` beside its
  provenance stamp, the run set defaults to the plan directory's name, and a failed
  run is retried with `--rerun`. Results that go into a report are run this way.
- `analyze.py`: mean G(t) +- SE over realizations (ratio estimator, delta-method
  SE), by cumulative dose (`G_cum`) and by final dose (`G_final`), on the common
  grid (`gvalues_time.csv`, plus one `gvalues_time_rate<r>.csv` per rate) and at
  beam-off (`gvalues_beamoff.csv`); several `--plan` options pool their runs; rows held
  by a steady-state stop are flagged `extrapolated`. `--runs-dir` (once per `--plan`)
  reads the outputs of a `-Study` run set; run it through `study.ps1 analyze` so the
  tables get an analysis stamp.
- `spike_report.py`: per-run wall time, peak memory, arrivals, dose reached,
  live molecules, steady-state stop, snapshot size and estimated full cost
  (`spike_table.md`, `spike_table.csv`).
