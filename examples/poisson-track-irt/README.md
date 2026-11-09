# Poisson-track IRT_syn example

A standalone Geant4-DNA example: several primary electron tracks, each transported
separately, share ONE IRT_syn water-radiolysis history, each activating at its own
time. The arrival times are either a fixed list or a Poisson process (rate, exposure,
separate source and chemistry seeds).

This is an engineering example. Its checks show that the bookkeeping is right
(activation times, identities, energy, preservation of earlier molecules, separated
sources adding up). It is not a calibrated or validated scientific result, and it
shows no dose-rate effect. It does not use any `dnachem-min` source or header; the
rest of the repository and the main application are unchanged by it.

## Approach and scope

1. `capture` transports each primary separately (Geant4 physics and pre-chemistry,
   `G4EmDNAPhysics_option2`, `G4EmDNAChemistry_option3`) and stops at 1 ps. The
   molecules present then are written as one **packet** file per primary, plus an
   index (`library.csv`) with the per-packet energy ledger.
2. `history` reads an arrival schedule (deterministic file or Poisson draw), shifts
   each packet by its offset and pushes all of them, before the chemistry starts,
   into one standalone IRT_syn chemistry run. A packet arriving at `t` activates at
   `t + 1 ps`, among the molecules the earlier packets left, and reacts with them.
3. The species counts are recorded on a log time grid after every arrival, and an
   observer checks every activation (see "Outputs").

Limits of the design, by intention:

- IRT_syn only. Classic IRT and SBS are rejected (`capture` and `history` exit with
  status 4 or a message naming IRT_syn).
- Primaries are never transported inside the scheduler, and packets are not
  streamed: the whole schedule is loaded before the chemistry runs.
- Pure water, no bulk scavenger (no O2, so no oxygen depletion).
- The mesoscopic (beam-off) stage is not part of this example.
- No pulse structure, no dose-rate conversion, no production run.
- Poisson counts are not conditioned: zero arrivals is a valid result (it writes zero
  counts on the grid). A packet is used at most once.
- Packets are translated only, not rotated.

## Dependencies

- A **patched** Geant4 11.4.3 with the DNA datasets (see the next section). An
  unpatched install does not compile `history`: the methods below do not exist.
- CMake 3.16 or newer, a C++20 compiler (built and tested here with MSVC 19 x64,
  Ninja, `RelWithDebInfo`). A vcpkg toolchain file is only needed if your Geant4 was
  built with vcpkg dependencies (HDF5, zlib), as here.
- PowerShell 7 for `scripts/RunSeparatedCheck.ps1` (optional).

Runtime: the Geant4 `bin` directory of the patched install must come first on `PATH`
so that no other Geant4 DLLs are loaded (ctest does this for you through
`ENVIRONMENT_MODIFICATION` on Windows), and the Geant4 data must be found, for
example through `GEANT4_DATA_DIR` (the directory holding `G4EMLOW...`,
`G4PARTICLEXS...`, ...) or the individual `G4*DATA` variables.

## Geant4 patches

The example calls two static methods that are not in the stock tree
(`G4DNAIndependentReactionTimeStepper::ResetSourceActivations()` and
`RegisterSourceActivation(trackID, activationTime)`; a search of the 11.4.3 source
finds neither), and it uses a corrected reflecting-box fold
(`G4ChemReboundTransportation::calculateNextCoordinate`, used by
`HistoryBoundedBrownianAction` and by the transport inside the box).

The patches are in the repository, for Geant4 11.4.3 only:

| order | file under `patches/geant4/11.4.3/` | provides |
|---|---|---|
| 1 | `molecular-finder/coincident-neighbors.patch` | octree/finder fix for coincident molecules (`G4Octree.icc`, `G4OctreeFinder.icc`, `G4DNAMakeReaction.cc`) |
| 2 | `timed-injection/source-contacts-experimental.patch` | `ResetSourceActivations`, `RegisterSourceActivation` (`G4DNAIndependentReactionTimeStepper.{hh,cc}`) |
| 3 | `timed-injection/meso-boundaries-experimental.patch` | bounded-box fold fix in `G4ChemReboundTransportation.cc` (also touches `G4DNAEventScheduler`, unused here) |

All three together change nine files and are what the evidence of this repository was
produced with. I did not test a smaller subset. The patches are experimental; see
`patches/geant4/11.4.3/timed-injection/README.md`.

Recipe (never patch your shared Geant4):

1. Copy the Geant4 11.4.3 source tree to a new directory `<private-source>`, without
   its `.git`. Run `git init` **inside** that copy (so `git apply` is not captured by
   an enclosing repository; a patch was silently skipped once for that reason).
2. Apply in the order above:
   `git -C <private-source> -c core.autocrlf=false apply --verbose <patch>`.
   Check that all nine paths were really changed.
3. Configure and build that copy into its own prefix, with the options of your normal
   Geant4 build (the one used here: Release, Ninja, multithreaded on, system zlib and
   HDF5 through vcpkg, Qt off, no data download). Install to `<patched-geant4-install>`.
4. Use `<patched-geant4-install>/lib/cmake/Geant4` as `Geant4_DIR` below.

Where this repository's evidence was produced:
`.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install` (built by `Build.cmd`
next to it). It is read-only evidence, not a path to rely on. Background:
`docs/dose-rate/beam-on-irt.md`, `docs/dose-rate/recurring-chemistry-sources.md`.

## Build and test

From the repository root (Windows: use a Visual Studio x64 developer shell):

```powershell
cmake -S examples/poisson-track-irt -B <build> -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake `
  -DGeant4_DIR=<patched-geant4-install>/lib/cmake/Geant4
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

`ctest` runs 29 tests in about 35 s (unit tests, capture and history smoke runs,
refusals). Executables: `capture`, `capture-check`, `history`, `compare`.

For the manual commands below, put the patched install's `bin` first on `PATH`
(`$env:PATH = "<patched-geant4-install>/bin;$env:PATH"`) and set `GEANT4_DATA_DIR`.

## Run the example

Use a work directory `<work>` outside the source tree. The config files find the
library with a relative path (`libraryDir = ../library`), so copy `config/` there:

```powershell
cd examples/poisson-track-irt
mkdir <work>; copy-item -Recurse config <work>/config
```

### 1. Capture the packets

```powershell
<build>/capture --macro macros/capture.mac --output <work>/library --count 16 --first-seed 1001
<build>/capture-check <work>/library --expect-count 16
```

`--output` must be empty or new. Packet `i` is made with physics seed
`first-seed + i`, so each packet is reproducible from its seed alone. Seeds must be in
1..2147483647. The macro (`macros/capture.mac`) sets the capture box
(`/ptirt/capture/halfSide 10 um`), `/run/initialize` and the gun (10 keV `e-` from
the box centre along +z). Do not put `/run/beamOn` or `/gun/time` in it (both are
errors), and do not select another time-step model.

Output in `<work>/library`: `packet-00000.txt` ..., `library.csv`, `capture.json`.
`capture` prints `CAPTURE_DONE packets=16 ...` and exits 0. For the commands above:
16 packets, 17359 molecules, 160 keV deposited, largest energy residual 4.7e-10 eV.
`library.csv` columns: `packetIndex,file,physicsSeed,primaryParticle,primaryEnergyEv,`
`energyDepositEv,escapedEnergyEv,energyResidualEv,moleculeCount`. Packet files store
every real as a hexadecimal float, so a read-back is bit exact. A packet carries a
compatibility tag (example version, Geant4 version, physics, chemistry, model,
capture age); `history` rejects a library with another tag.

### 2. Shared history, deterministic schedule

```powershell
<build>/history --config <work>/config/deterministic.cfg --output <work>/out/deterministic
```

`config/deterministic-schedule.txt` has three 10 keV tracks about 100 um apart,
arriving at 0, 0.5 and 1 ms; the history runs to 3 ms (about 8 s here).
`--output` must be empty or new. The console ends with `HISTORY_DONE mode=deterministic
sources=3 molecules=3216 ...` and the exit status is 0.

### 3. Single-source control

```powershell
<build>/history --config <work>/config/single.cfg --output <work>/out/single
```

This injects row `singleSource` (1) of the same schedule alone, at its scheduled
time, on the record grid of the full schedule (so its `species.csv` lines up row by
row with the deterministic one). Its count is 0 before its activation. Use
`singleSource = 0, 1, 2` and a distinct `chemistrySeed` for each control.

### 4. Poisson arrivals

```powershell
<build>/history --config <work>/config/poisson.cfg --output <work>/out/poisson
```

2 kHz for a 2 ms exposure (4 tracks on average), placed uniformly in a 100 um box,
observed to 3 ms (about 28 s here; the recorded run drew 6 arrivals with `sourceSeed
= 1`). Arrival `k` uses packet `k`: the library needs at least as many packets as
arrivals, else the run stops with "library exhausted" and names the arrival.
`sourceSeed` draws the arrivals (exponential waits, then x, y, z, with a
`std::mt19937_64`), `chemistrySeed` drives the chemistry; they are independent, so a
schedule can be replayed with another chemistry seed and the reverse.

### Exit status

`history` and `capture` print `HISTORY_ERROR <reason>` / `CAPTURE_ERROR <reason>` on
stderr and write nothing else on a refusal. History: 2 bad arguments, config,
schedule or packets (including a malformed or incompatible packet), 3 initialization
failure, 4 the chemistry model is not IRT_syn, 1 a failed check or a failure during
the run. Capture: 2 bad arguments, 3 macro or initialization failure, 4 not IRT_syn,
1 failure during the run.

## History configuration

One `key = value` per line; `#` at the start of a line is a comment; blank lines are
ignored. Every key at most once. An unknown key, a key the mode does not use, an empty
value or a malformed number is an error naming the line. Relative paths are relative to
the config file's directory. Times in the config are ns unless the key says otherwise.

| key | mode | unit | default | meaning |
|---|---|---|---|---|
| `mode` | all | | required | `deterministic`, `single` or `poisson` |
| `libraryDir` | all | path | required | packet library written by `capture` |
| `endpointNs` | all | ns | required | end of the history, > 0 |
| `chemistrySeed` | all | | required | 1..2147483647 |
| `scheduleFile` | deterministic, single | path | required | arrival list (format below) |
| `singleSource` | single | row index | required | schedule row to inject alone (0-based) |
| `rateHz` | poisson | 1/s | required | mean arrival rate, > 0 |
| `exposureS` | poisson | s | required | arrivals fall in `[0, exposureS)`; must end before `endpointNs` |
| `sourceSeed` | poisson | | required | 1..2147483647 |
| `regionCenterUm` | poisson | um | `0 0 0` | centre of the placement box (x y z) |
| `regionHalfSizeUm` | poisson | um | `50` | half extent, one value or x y z, >= 0 (0 pins that axis) |
| `worldHalfSideUm` | all | um | `1000` | half side of the water world; every molecule must lie strictly inside it |
| `halfBoxUm` | all | um | `0` | reflecting chemistry box half side; 0 = none; must be smaller than the world |
| `gridMinAgeNs` | all | ns | `1` | first record age after each arrival, >= 0.01 |
| `gridPointsPerDecade` | all | | `5` | record points per decade of age, 1..100 |

Record grid: for the anchor 0 and for every arrival time `a`, the times
`a + gridMinAgeNs * 10^(k / gridPointsPerDecade)` below the endpoint, in absolute time,
plus the endpoint. A point closer than 5 ps to an arrival time is dropped, so no record
falls on the step where a new source is counted. Duplicates that are exactly equal are
removed; nothing else is merged here (see "Known limits"). More than 1,000,000 points is
an error.

### Schedule file (deterministic and single)

```
# time_ns  packet_index  dx_um  dy_um  dz_um
0          0             0      0      0
500000     1             100    0      0
```

Arrival time in ns, library packet index (0-based), translation in um. Rules: finite
numbers, time >= 0 and below the endpoint, times nondecreasing, packet index below the
library size, each packet index at most once, between 1 and 100000 rows. A violation
is reported with its line number.

## Outputs

`capture`: `packet-NNNNN.txt`, `library.csv` (above), `capture.json` (human-readable
summary: tag, versions, seeds, totals, per-packet origin, direction and energies).

`history` writes three files into `--output`:

- `species.csv`: long layout, columns `timeNs,species,count,history,chemistrySeed`;
  one row per record time and species, in absolute time. The 16 species, in Geant4's
  spelling, are `H2O2^0`, `H3O^1`, `HO_2^-1`, `HO_2°^0`, `H^0`, `H_2^0`, `OH^-1`, `O^-1`,
  `O^0`, `O_2^-1`, `O_2^0`, `O_3^-1`, `O_3^0`, `e_aq^-1`, `°OH^0`, `°O^0` (rows are
  sorted by time, then name). The default schedule
  gives 97 record times and 1552 rows. Counts are of the molecules of the shared
  history; a source is counted from its activation.
- `activations.csv`: one row per source: `sourceIndex,packetIndex,offsetXNm,offsetYNm,`
  `offsetZNm,scheduledTimeNs,expectedActivationTimeNs,observedActivationTimeNs,`
  `firstTrackId,lastTrackId,moleculeCount,energyDepositEv,escapedEnergyEv`. Expected
  and observed times agree exactly (scheduled time + 1 ps); track IDs are positive,
  contiguous, in schedule order.
- `ledger.json`: seeds, settings, library totals, injected totals (energy: primary,
  deposit, escaped), one entry per source (with its `physicsSeed`), and `checks`:
  `allPassed`, `chemistryRan`, the activation stages (per stage: time, sources,
  `injected`, `preserved`), the preservation total, the end checks (`steps`,
  `finalTime_ns`, live molecules, `allLiveAtEndTime`, `counterMatchesLive`,
  `allInsideBox`) and `reflectingBoxFolds` (dissociation and reaction products
  folded back into the box). Also `wallTime_s`.

The recorded deterministic run: 3 stages, 1083 / 1036 / 1097 molecules injected,
preserved 0 / 752 / 1462 earlier molecules at the stages, all checks passed,
`allPassed: true`.

## Adapting it: the hooks

The Geant4-free layer (`ptirtCore`: packets, schedule, config, ledger) has no Geant4
include and is covered by plain unit tests. To adapt:

| to change | where | what |
|---|---|---|
| primary, energy, direction | `macros/capture.mac` (`/gun/...`) | any single particle of a Geant4-DNA water model |
| capture geometry | `CaptureDetectorConstruction` (`/ptirt/capture/halfSide`) | one `G4_WATER` box; what leaves it is counted as escaped energy |
| physics and chemistry | `ExamplePhysicsList` and `ExamplePhysicsTag.hh` | if you change physics, chemistry model, medium or capture age, change the compatibility tag so old libraries are refused |
| per-event capture | `CaptureRecorder` (`BeginEvent`, `AddDeposit`, `EndTrack`, `CaptureAtChemistryStart`, `EndEvent`) and `CaptureActions` | what a packet holds and how it is validated |
| arrival schedule | `ArrivalSchedule.hh` | produce a `std::vector<ptirt::Arrival>` (`timeNs`, `packetIndex`, `offsetNm`) yourself, or reuse `ParseDeterministicSchedule` / `SamplePoissonArrivals`; the rest of `history` only needs that vector |
| where the packets are placed | `ptirt::SourceInjector(arrivals, library, worldHalfSide, boxHalfSide, endpoint)` then `Inject()` | validates everything before any Geant4 state changes, assigns positive track IDs, calls `ResetSourceActivations` / `RegisterSourceActivation` (the patched API), pushes into `G4ITTrackHolder` before `G4DNAChemistryManager::Run()` |
| what is observed or written | `ActivationObserver` (a `G4UserTimeStepAction`): `Configure(sources, recordTimes, box, counterId)`, then `SpeciesRows()`, `Activations()`, `End()`, `Failure()`; and the writers in `Ledger.hh` (`WriteSpeciesCsv`, `WriteActivationsCsv`, `JsonValue`) | add observables in `UserPostTimeStepAction` / `EndProcessing`, columns in `Ledger.cc` |
| record grid | `BuildRecordGrid` in `HistoryConfig.hh` | |
| config keys | `HistoryConfig.cc` | one place for parsing and validation |
| world and box | `HistoryDetectorConstruction`, `ExamplePhysicsList` (reflecting box), `HistoryBoundedBrownianAction` | |

The driver that wires these together is `src/HistoryMain.cc` (read it first, about
500 lines); `src/CaptureMain.cc` does the same for capture.

## Separated-sources check

`scripts/RunSeparatedCheck.ps1` checks that a combined history of three sources about
100 um apart matches, within statistics, the sum of the same sources run alone
(16 combined and 3 x 16 single histories, tolerance, controls and result in
[docs/separated-check.md](docs/separated-check.md)):

```powershell
pwsh examples/poisson-track-irt/scripts/RunSeparatedCheck.ps1 -BuildDir <build> -OutDir <new empty dir>
```

It takes about 75 s with 6 histories in parallel (each capped at 30 min), refuses a
non-empty output directory, and prints `SEPARATED_CHECK_PASS` and
`NEGATIVE_CONTROL_OK` when it succeeds.

## Known limits

- The reflecting-box fold is never exercised by the tests: the box smoke test and the
  manual box run both end with `reflectingBoxFolds` at 0, so a product placed outside
  the box and folded back is untested here. Only `allInsideBox` is checked.
- With a box (`halfBoxUm > 0`) Geant4 prints many `ProcMan113` warnings ("Set Ordering
  First is invoked twice for ChemReboundTransportation ..."). They were judged
  benign and no result depended on them.
- No ctest checks that a `history` run is reproducible from its seeds (capture packets
  are checked this way).
- Premature, missing and duplicate activations are checked only by the Geant4-free
  unit test `ActivationObserverCheckTest`; no end-to-end run provokes them.
- The record grid removes exact duplicates only. Times closer than 1 ps are merged
  by the molecule counter, not by the grid.
- A library with a partial packet (an aborted capture) is not repaired: loading it
  fails loudly, by design.
- The timeout and kill path of `RunSeparatedCheck.ps1` has not been exercised.
- The separated check covers sources about 100 um apart, pure water, no O2 and so no
  depletion. It says nothing about overlapping tracks, oxygen, or a significant
  dose-rate effect.
- The Geant4 patches are experimental and for 11.4.3 only. The example was built and
  run on Windows with MSVC only.
