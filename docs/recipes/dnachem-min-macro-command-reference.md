# dnachem-min macro command reference

## What this is

A reference listing of every custom `/command` dnachem-min defines, grouped
by directory, with each command's allowed application state(s) and the
source file that creates it. This is reference content, not a how-to — see
[`create-a-macro-command.md`](create-a-macro-command.md) for how to add a new
one. Every entry below was grep-confirmed against the actual
`new G4UIcmd...`/`DeclareProperty`/`DeclareMethod...` call in its source
file, not transcribed from `CLAUDE.md`'s narrative summary alone — it must
not silently drift from or contradict that summary.

## `/run/*`

| Command | State(s) | Source |
|---|---|---|
| `/run/outputDir <path>` | PreInit | `src/core/OutputDirMessenger.cc (OutputDirMessenger constructor)` |
| `/run/dumpDataAndReset [prefix]` | Idle | `src/scoring/RunAccumulatorMessenger.cc (RunAccumulatorMessenger constructor)` |
| `/run/dumpDataAndResetToDir <subdir>` | Idle | `src/scoring/RunAccumulatorMessenger.cc (RunAccumulatorMessenger constructor)` |

`outputDir` is the macro-file counterpart of the `--dir` CLI flag
(`OutputDir::ConfigureFromMacro`); issuing it with a value conflicting with
one already set (by `--dir` or an earlier `/run/outputDir`) is a fatal
`G4Exception`. The two dump commands write every output file accumulated
since the last dump (or program start) and a `Manifest.json`, then reset the
accumulators; `dumpDataAndReset`'s optional `prefix` is prepended literally
to every filename, `dumpDataAndResetToDir`'s `subdir` writes into a
subfolder instead. Both are fatal if the prefix/subdir was already used
earlier in the run (`RunAccumulatorMessenger::SetNewValue`).

## `/chem/select`, `/chem/list`

| Command | State(s) | Source |
|---|---|---|
| `/chem/select <name>` | PreInit | `src/chemistry/ChemistrySelectMessenger.cc` |
| `/chem/list` | PreInit, Idle | `src/chemistry/ChemistrySelectMessenger.cc` |

`/chem/select` picks the named Chemistry (case-insensitive, default
`PureWater`); an unknown name, or selecting a different name twice, is a
fatal `G4Exception` (`ChemistryRegistry::Select`). `/chem/list` just prints
the registered names with the default marked, so it is also allowed in
`Idle` — it reads state, it does not configure anything.

## `/chem/env/*`

| Command | State(s) | Source |
|---|---|---|
| `/chem/env/pH <double>` | PreInit | `src/geometry/DnaChemistryWorld.cc` |
| `/chem/env/halfBox <value> <unit>` | PreInit | `src/geometry/DnaChemistryWorld.cc` |
| `/chem/env/scavenger <species> <value> <unit>` | PreInit | `src/geometry/ScavengerMessenger.cc` |

`pH` and `halfBox` are declared on a `G4GenericMessenger` owning
`/chem/env/` (`DnaChemistryWorld.cc`): `pH` via `DeclareProperty`
(binds straight to `fpH`), `halfBox` via `DeclareMethodWithUnit` (binds to
`SetHalfBox`, which validates `> 0` and rebuilds the chemistry boundary — a
fatal `G4Exception` below or equal to 0). `scavenger` is a plain
`G4UIcommand` with three string parameters (`ScavengerMessenger.cc`
— strings let `ScavengerSpec::Parse` report invalid numeric values as fatal
errors; Geant4 can reject missing required tokens before invoking the parser),
attached to the same `/chem/env/` directory but constructed *after*
`DnaChemistryWorld`'s own `fMessenger` so that directory already exists
(`DnaChemistryWorld.cc`).

## `/chem/meso/*`

| Command | State(s) | Source |
|---|---|---|
| `/chem/meso/handOverTime <value> <unit>` | PreInit | `src/chemistry/MesoMessenger.cc` |
| `/chem/meso/voxelSize <value> <unit>` | PreInit | `src/chemistry/MesoMessenger.cc` |
| `/chem/meso/timesPerDecade <n>` | PreInit | `src/chemistry/MesoMessenger.cc` |
| `/chem/meso/spatialOutput <bool>` | PreInit | `src/chemistry/MesoMessenger.cc` |
| `/chem/meso/enable <bool>` | PreInit | `src/chemistry/MesoMessenger.cc` |

All five are typed `G4UIcmdWith*` objects built in `MesoMessenger`'s
constructor, writing into the process-wide `MesoSettings::Current()`.
`handOverTime`, `voxelSize` must be `> 0` and `timesPerDecade` must be `>= 1`
— each a fatal `G4Exception` otherwise (`MesoMessenger.cc`). The
`SBS`/`enable true` conflict is deferred to `/run/initialize`
(`DnaChemistryList::CheckTimeStepModel`), not checked here, because the
time-step model may not be set yet when `enable` is issued
(`MesoMessenger.cc`).

## `/chem/reaction/*`

| Command | State(s) | Source |
|---|---|---|
| `/chem/reaction/dump <filename>` | PreInit | `src/chemistry/DnaChemistryList.cc` |
| `/chem/reaction/timeBinsFixed <width> <unit>` | PreInit | `src/chemistry/DnaChemistryList.cc` |
| `/chem/reaction/timeBinsList <e1> ... <eN> <unit>` | PreInit | `src/chemistry/DnaChemistryList.cc` |

All three are declared on a `G4GenericMessenger` owning the new
`/chem/reaction/` directory (`DnaChemistryList.cc`). `dump` and
`timeBinsList` use `DeclareProperty`; `timeBinsFixed` uses
`DeclareMethodWithUnit`. `timeBinsList` is deliberately `DeclareProperty`
rather than `DeclareMethod` — see
[`create-a-macro-command.md`](create-a-macro-command.md)'s truncation
pitfall section — so its `"<e1> ... <eN> <unit>"` string survives intact
for `DnaChemistryList::ApplyReactionTimeBinning()` to parse later, once the
scheduler end time is final. Issuing both `timeBinsFixed` and
`timeBinsList` is a fatal `G4Exception` at that later parse point, not at
either command itself (`DnaChemistryList.cc`).

## `/dnaLogger/verbose`

| Command | State(s) | Source |
|---|---|---|
| `/dnaLogger/verbose Quiet\|Error\|Warning\|Info\|Debug\|Trace` | PreInit, Idle, GeomClosed, EventProc | `src/core/DnaLoggerMessenger.cc (DnaLoggerMessenger constructor)` |

A plain `G4UIcmdWithAString` with `SetCandidates(DnaLogger::LevelCandidates())`
restricting the argument to the named levels. Unlike every other custom
command in this project, it is explicitly allowed in `GeomClosed` and
`EventProc` as well as `PreInit`/`Idle` — it is meant to be toggled mid-run
("use `/dnaLogger/verbose Debug` for detailed diagnostics", per
`CLAUDE.md`), not just at setup.

## `/primaryKiller/*`

| Command | State(s) | Source |
|---|---|---|
| `/primaryKiller/eLossMin <value> <unit>` | Geant4 defaults (see below) | `src/scoring/PrimaryKiller.cc (PrimaryKiller constructor)` |
| `/primaryKiller/eLossMax <value> <unit>` | Geant4 defaults (see below) | `src/scoring/PrimaryKiller.cc (PrimaryKiller constructor)` |
| `/primaryKiller/minKineticE <value> <unit>` | Geant4 defaults (see below) | `src/scoring/PrimaryKiller.cc (PrimaryKiller constructor)` |
| `/primaryKiller/setSize <x> <y> <z> <unit>` | Geant4 defaults (see below) | `src/scoring/PrimaryKiller.cc (PrimaryKiller constructor)` |
| `/primaryKiller/verbose <n>` | Geant4 defaults (see below) | `src/scoring/PrimaryKiller.cc (PrimaryKiller constructor)` |

None of `PrimaryKiller`'s five commands calls `AvailableForStates`, so each
keeps the default state list initialized in Geant4 11.4.1
`source/intercoms/src/G4UIcommand.cc`: PreInit, Init, Idle, GeomClosed,
EventProc and Abort. `eLossMin`
(`/primaryKiller/eLossMin`) and `eLossMax` set the primary-kill and event-abort
thresholds in `PrimaryKiller::SetNewValue`. The registered `minKineticE` command
currently has no dispatcher branch, so it does not change the threshold; the
C++ `SetEnergyThreshold` setter does. `setSize` sets the bounding box any track is
killed for leaving, default `1 km` per side; `verbose 1` logs each
`kill track at` line. `CLAUDE.md` additionally documents that
`DetectorConstruction::ConstructSDandField()` registers this scorer
unconditionally with hardcoded non-trivial defaults (`eLossMin` = 500 keV,
`eLossMax` = 1 MeV, `src/geometry/DetectorConstruction.cc`) that
already apply before any macro overrides them.

## Built-in commands this project relies on

| Command | Notes |
|---|---|
| `/process/chem/TimeStepModel SBS\|IRT_syn` | PreInit, before `/run/initialize`. Read by `DnaChemistryList::CheckTimeStepModel()` (`DnaChemistryList.cc`, called from `ConstructProcess` at `/run/initialize`) via `G4EmParameters::Instance()->GetTimeStepModel()`; any value other than `IRT_syn`/`SBS` is a fatal `G4Exception`. An explicit `/chem/meso/enable true` together with `SBS` is also fatal (`DnaChemistryList.cc`). |
| `/scheduler/endTime <value> <unit>` | Must be issued *after* `/run/initialize`: `CheckTimeStepModel()` hardcodes the default end time (1 s with the mesoscopic stage, 1 us without) via `G4Scheduler::Instance()->SetEndTime(...)` at that point (`DnaChemistryList.cc`, also set again in `ActionInitialization.cc`) — a value set earlier is overwritten. |

These are not dnachem-min commands (no project messenger creates them); they
are standard Geant4-DNA chemistry-kernel commands this project depends on at
specific points in the macro, as already documented narratively in
`CLAUDE.md`.

## Serial vs MT

Eligible master commands are queued and replayed through worker UI managers;
actual dispatch depends on the receiving command tree and application state.
See [`create-a-macro-command.md`](create-a-macro-command.md#serial-vs-mt).

`/chem/env/scavenger` explicitly calls `SetToBeBroadcasted(false)` in
`ScavengerMessenger`'s constructor. Its master-side configuration updates the
shared `DnaChemistryWorld` before workers read that composition. The world is
not separate master-only storage. This command is excluded from the normal
broadcast queue (`G4UImanager::ApplyCommand`, Geant4 intercoms source).
Do not infer identical messenger callbacks or safe concurrent writes from
the default broadcast flag on other commands.

## See also

- [`create-a-macro-command.md`](create-a-macro-command.md) — how to add a
  new command, the two messenger styles, the `DeclareMethod` truncation
  pitfall, and the general Serial vs MT replay mechanism this reference
  links to above.
- [`docs/adr/0008-selectable-chemistry-modes.md`](../adr/0008-selectable-chemistry-modes.md)
  — `/chem/select`, `/process/chem/TimeStepModel` and `/chem/meso/enable` in
  context.
- [`docs/adr/0004-scavenger-reactions-per-chemistry.md`](../adr/0004-scavenger-reactions-per-chemistry.md)
  — how `/chem/env/scavenger` reacts through the selected Chemistry's bulk
  reactions.
