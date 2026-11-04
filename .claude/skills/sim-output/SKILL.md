---
name: sim-output
description: Use when running sim.exe, writing macros that dump output (/run/dumpDataAndReset, /run/dumpDataAndResetToDir, /run/outputDir, --dir, /output/...), analyzing its output files, or changing output/scoring code (RunAccumulator, OutputDir, counters, ProductOutput).
---

# sim.exe output files and dump commands

Species/reaction/energy/physical-interaction data accumulates across `/run/beamOn`
calls rather than being written and reset after every run: species yields
accumulate automatically (the `ScoreSpecies` scorer is itself a persistent,
SD-registered object), while energy deposit, the counters and the track-length table accumulate via
`RunAccumulator` (`src/scoring/RunAccumulator.cc`), fed once per run from
`RunAction::EndOfRunAction`. Pre-chemical and spatial data are staged
during events. Final dump files are published when a macro issues
`/run/dumpDataAndReset [prefix]` (`Idle` state, i.e. between `beamOn` calls),
which writes everything accumulated since the last dump (or program start) and
resets all counters to empty/zero.

## Output products, formats and names

Seven **Output products**, each written as `<Product>.<ext>` in the formats
chosen for it (several at once allowed; the extension is the format name,
lower case): `txt`, `csv`, `h5`, `root`. The built-in default is `csv`.

```text
/output/defaultFormat csv h5             # default for products without their own command; 'none' = no file
/output/<product>/format txt csv h5 root # replaces the default for one product; 'none' = no file
```

Both are PreInit (before `/run/initialize`). `<product>` is one of `species`,
`speciesMeso`, `reactions`, `physicsInteractions`, `trackLengths`,
`preChemical`, `speciesMesoSpatial`. Formats are case-insensitive, listed in
canonical order; `none` cannot be combined with another format.
`speciesMesoSpatial` accepts only `h5`/`root`, never follows the default and
writes nothing unless its own command is given. A product skips default
formats it does not accept. Chosen lists are in `Manifest.json`
`outputFormats`; every written file is in `files`.

Failure modes: an unknown format, a format the product does not accept, `none`
mixed with others, or `h5` in a build without HDF5 (`hdf5: false`) is a fatal
`G4Exception` (`InvalidOutputFormat`) at the command. Under the default list a
build without HDF5 silently drops `h5`. A file that cannot be written is a
`ProductOutputWriteFailed` warning naming the file; the other files are still
written. Layouts of h5/root: `docs/output/tables-h5-root.md`.

File naming: `txt`/`csv` write one file per table (`Reactions.csv` and
`ReactionsCatalog.csv`); `h5`/`root` write one `<Product>.<ext>` holding every
table of the product (h5: group per table, dataset per column with `unit` and
`description` attributes; root: TTree per table, integers stored as doubles,
unit in the leaf title). The prefix or subfolder of the dump applies to all
names.

## Common table conventions

Event-wise products share the statistic columns: `nEvents` (non-aborted events
of the dump; absent species/reactions/processes count as 0 in an event),
`countSum`, `countSumSq`, `countMean` = countSum/nEvents, `countStdErr` =
sqrt((countSumSq/n - mean^2)/(n-1)) (0 when n < 2). Sums, not means, are
the mergeable quantities: add `countSum`/`countSumSq`/`nEvents` across dumps and
recompute. G-values are per event, molecules per 100 eV of that event's energy
deposit (an event with zero deposit adds counts but G = 0).

## Products and columns

- **Species** (`Species.<ext>`; table `species`): molecule counts and G-values
  of the particle-based stage (up to the hand-over time; the whole run when the
  mesoscopic stage is off). Columns: `time_ns` (ns), `species`,
  `nEvents`, `countSum`, `countSumSq`, `countMean`, `countStdErr`, `gSum`,
  `gSumSq`, `gMean`, `gStdErr` (G columns in molecules/100 eV and its square).
  Long layout: one row per (time, species), by time then species. The
  Chemistry decides the species (`Tonneau2025` adds `HO3`).
- **SpeciesMeso** (`SpeciesMeso.<ext>`; table `speciesMeso`): the same columns
  for the mesoscopic stage (hand-over time, default 5 ns, to the end time), on
  the log record grid (`/chem/meso/timesPerDecade`, default 10). Bulk species
  are not listed. Not written when the mesoscopic stage is off
  (`mesoEnabled: false`: SBS, or `/chem/meso/enable false`); with end time at or
  before the hand-over time it is written with only its header.
- **Reactions** (`Reactions.<ext>` + `ReactionsCatalog.<ext>`; tables
  `reactions`, `reactionCatalog`): per-time-bin firing counts of each
  bimolecular reaction, counted in `TimeStepAction::UserReactionAction`.
  `reactions`: `reactionId`, `binStart_ns` (0 for the first bin), `binEnd_ns`
  (ns), `nEvents`, `countSum`, `countSumSq`, `countMean`, `countStdErr`.
  `reactionCatalog`: `reactionId`, `reaction` (`"A + B -> C + D"`, ids sorted
  by label). Particle-based stage only; acid-base/scavenger reactions are not
  counted. Bin edges: a built-in 7-entry table, overridden by
  `/chem/reaction/timeBinsFixed <width> <unit>` or `/chem/reaction/timeBinsList
  <e1> ... <eN> <unit>` (PreInit, mutually exclusive; both is fatal at
  BeginOfRunAction).
- **PhysicsInteractions** (`PhysicsInteractions.<ext>`): per discrete `G4DNA`
  process (`e-_G4DNAIonisation`, `e-_G4DNAExcitation`, `e-_G4DNAElastic`,
  `e-_G4DNAVibExcitation`, `e-_G4DNAAttachment`, ...; `Transportation` excluded),
  recorded per step by `SteppingAction`. Columns: `process`, `nEvents`,
  `countSum`, `countSumSq`, `countMean`, `countStdErr`.
- **TrackLengths** (`TrackLengths.<ext>`): one row per non-aborted event, by run
  then event. Columns: `run`, `event`, `primaryLength_nm` (nm),
  `primaryEkinStart_keV`, `primaryEkinEnd_keV` (keV), `primaryEnd` (`stopped`,
  `escaped` or `killed` by `PrimaryKiller`), `secondaryFirstGenLength_nm` (nm,
  electrons other than track 1 whose parent is track 1), `nSecondaryFirstGen`,
  `secondaryAllLength_nm`, `nSecondaryAll` (every non-primary electron, PDG 11).
  Events aborted by `/primaryKiller/eLossMax` are skipped. The manifest's
  `trackLengths` object summarises the rows (`events`, end-reason counts,
  `mean`/`sem`, `null` without events).
- **PreChemical**: species created by the physical stage, one row each. Columns:
  `run`, `event`, `parentTrackId`, `molecule` (`H2O` or `e_aq`), `modification`
  (`ionisation`, `excitation`, `dissociativeAttachment` for H2O; `solvation` for
  e_aq), `level` (electronic level, -1 for e_aq), `energy_eV` (eV: deposited
  for H2O, kinetic for e_aq), `parentX_nm`, `parentY_nm`, `parentZ_nm` (incoming
  track), `x_nm`, `y_nm`, `z_nm` (species), all nm. txt/csv: one file per event,
  `PreChemical_run<R>_event<E>.<ext>` (run and event start at 0, the run
  index keeps counting across dumps), staged in `<outdir>/.pending_prechem/`
  by `EventAction`; h5/root: one `PreChemical.<ext>` per dump, rows kept in
  memory until the dump, staged there too. Every dump moves the staged files into
  the dump (prefixed, or inside the subfolder) in numeric (run, event) order and
  lists them in `Manifest.json` `files`. A failed move is a
  `PreChemicalMoveFailed` warning and the file stays staged. `none` stages nothing.
- **SpeciesMesoSpatial** (`SpeciesMesoSpatial.h5` and/or `.root`): spatial
  snapshots of the mesoscopic mesh, only with `/output/speciesMesoSpatial/format`
  (the old `/chem/meso/` switch is gone). h5: nested
  `/run<R>/event<E>/snapshot<k>/` with attributes `time_ns`, `cellSize_nm` and
  datasets `position_nm` (N x 3 float64, cell centres) and `counts` (N x S
  uint32), non-empty cells only, appended per event by `TimeStepAction` into
  `<outdir>/.pending_meso_spatial/`; root: one long tree with columns `run`,
  `event`, `snapshot`, `time_ns`, `cellSize_nm`, `x_nm`, `y_nm`, `z_nm`,
  `species`, `count` (> 0), rows held in memory until the dump. Every dump
  moves the staged file(s); a failed move is a `MesoSpatialMoveFailed`
  warning. Full format: `docs/output/SpeciesMesoSpatial-h5.md`. Directory trees
  per layout and one line per file: `docs/output/README.md`.
- `Manifest.json`: one per dump, describing what produced the data beside it
  (`RunManifest` builds a `DataNode` tree and is the only place listing the
  entries, the generic `JsonWriter` serialises it; `schemaVersion: 1`, units
  in the keys). Top level: `timestamp`, `elapsedSinceStart_s` (main start → dump),
  `elapsedSincePreviousDump_s` (previous dump or process start → dump),
  `geant4Version`, `outputFormats` (format list per product), `macro`, `chemistry`, `scavengers` (`species`, `molarity_M`), `pH`,
  `chemistryEndTime_ns` (1e9 for the 1 s end time with meso; 1000 for the 1 us
  default without it), `timeStepModel` (`"IRT_syn"` or `"SBS"`), `hdf5`
  (bool; false = built without HDF5, no h5 files), `mesoEnabled`
  (bool; false = no `SpeciesMeso*` files), `chemistryModel` (`"IRT_syn+mesoscopic"`,
  `"IRT_syn"` or `"SBS"`), `handOverTime_ns` (null without the mesoscopic stage), `voxelSize_nm` (requested cell
  size), `mesoPixels` (initial mesh pixels per side, capped at 65536, so on the
  default 1 mm box the cell actually used is 15.26 nm; null without a chemistry
  world), `mesoTimesPerDecade`, `runMode` (`Serial`/`MT`), `threads`,
  `outputDirAsConfigured`/`outputDirAbsolute`, `prefix`, `subdir`,
  `totalEvents`, `totalEnergyDeposit_eV` (sums over `runs`; there is no
  `EnergyDeposit.Txt`), `files` (the data files this dump wrote, relative
  to the manifest's folder, manifest excluded) and `runs[]`: one entry per
  `/run/beamOn` folded into the dump with `run`, `events`, `particle`,
  `beamEnergy_keV`, `position_um`, `direction`, `energyDeposit_eV`, `seed` and `wallTime_s`.
  The beam is what the gun actually had on the first event of that run, so
  several `/gun/energy` values between dumps show up as separate `runs[]`
  entries. `seed` is the random engine's seed as configured when the run
  started (it follows `/random/setSeeds`); output is still not bit-reproducible
  from it. It is written on every dump, including the `EndOfRun_` flush, and
  carries the same prefix or subfolder as the data files
  (`EndOfRun_Manifest.json`, `run01/Manifest.json`).

`prefix` (optional, default none) is prepended literally to every filename
above — no separator is inserted, so pass e.g. `run1_` if you want one.
Reusing a prefix already used earlier in the same `sim.exe` process is a
fatal error (`RunAccumulator::TryReservePrefix`), to catch accidental
overwrites.

`/run/dumpDataAndResetToDir <subdir>` (`Idle` state, parameter required) is the
same dump, but writes the unprefixed files into `<subdir>` under the output
directory (`--dir` / `/run/outputDir`, otherwise the lazy `<exeDir>/results`
default; cwd if executable lookup or lazy default-directory creation fails) instead of using a
filename prefix — e.g. `/run/beamOn 4`, `/run/dumpDataAndResetToDir run01`,
`/run/beamOn 4`, `/run/dumpDataAndResetToDir run02` yields `run01/` and
`run02/`, each with the full file set. The folder is created if missing
(`OutputDir::ConfigureSubdir`); nested names such as `scan1/run01` are allowed,
absolute paths and `..` components are a fatal `G4Exception`
(`InvalidDumpSubdir`). Reusing a name within the same process is a fatal
`G4Exception` (`DuplicateDumpSubdir`, `RunAccumulator::TryReserveSubdir` — a
set separate from the prefix one); a folder already on disk from an earlier
process is reused and its files overwritten. It does not combine with a
prefix. The staged `PreChemical` files are moved into the
subfolder like the other data files.

If a macro never issues `/run/dumpDataAndReset` and there is still accumulated
data pending when the program is about to exit, a safety-net flush fires
automatically with the fixed prefix `EndOfRun_` (see
`RunAccumulatorMessenger::FlushIfPending`, called from `sim.cc` just before
the run manager is destroyed). It attempts publication if the operator omits
the manual call; existing output failures can still lose data. The `[RunAccumulatorMessenger] dumped and
reset...` line is a plain, always-visible `G4cout` line (unlike most of this
project's diagnostics, which go through `DnaLogger` and are silent by
default) — see `RunAccumulatorMessenger.cc`.

Without `--dir` or `/run/outputDir`, output goes to `<exeDir>/results`
(`OutputDir::SetDefaultDir` from `sim.cc`, created lazily at the first output,
so a run that sets either never creates it; a `/run/outputDir` issued after the
default was already used is refused). `<outdir>/Manifest.json` is also the
**results index** (`ResultsIndex`, rewritten at every dump, the `EndOfRun_`
flush included): top level `kind: "resultsIndex"`, `outputDirAbsolute` and
`dumps[]` (`folder`, `prefix`, `manifest` = relative path of that dump's own
manifest, `timestamp`, `events`, `runs[]` with run/events/seed/beam). Each
sub-folder or prefixed dump keeps its own manifest unchanged. Collision: a flat
dump with an empty prefix has its manifest at the index's path, so that file
is that dump's manifest plus a top-level `dumps` array (no `kind`).

Pass `--dir <path>` to redirect every output file above (plus the
`/chem/reaction/dump` target, if the macro sets one) into `<path>` instead of
the default directory — useful for isolating each run's output when scripting many `sim.exe`
invocations. `<path>` itself is created if missing; its parent must already
exist. Put the macro filename first (`argv[1]`), followed by CLI flags:

```bash
./sim beam.in --dir runs/001
```

The macro command `/run/outputDir <path>` (`PreInit` only) is a second way
to set the same output directory, for when `--dir` isn't convenient (e.g. a
self-contained macro). `--dir` is always applied before the macro executes,
so if both are used with *different* paths, `/run/outputDir` raises a fatal
`G4Exception` (`ConflictingOutputDir`) and aborts the run rather than
silently picking one; the same path from both is a harmless no-op.
