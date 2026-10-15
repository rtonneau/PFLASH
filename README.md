# dnachem-min

Minimal Geant4-DNA example for studying water radiolysis from electron beams.
It provides the common physics, chemistry, and scoring basis for
conventional and ultra-high-dose-rate (UHDR) irradiation studies. UHDR is a
use case, not a separate implementation — there is no dedicated UHDR
pulse-structure, dose-rate-effect, or delivery-model code, and none is
planned unless the project scope changes.

The geometry is a single homogeneous water box (no voxelization).

## Requirements

- Geant4 11.0+ with DNA models
- CMake 3.16+
- A C++20 compiler
- HDF5 with C++ support

## Build

See `.claude/geant4-instructions.md` for the full MSVC build procedure
(the `build/` vs `build-ninja/` split, background runs, verification
checklist). In short: `build/` is a RelWithDebInfo tree used to run `sim`;
`build-ninja/` is a Debug tree used to run the unit tests via CTest.

## Run

From the run build directory (`build/`), pass the macro **filename only**.
The macro is looked up in this order: the argument as given, then
`<exeDir>/<arg>`, then `<exeDir>/macro/<arg>` (the build copies `macro/`
alongside the executable):

```bash
./sim beam.in             # pure-water radiolysis
./sim beam_o2.in          # with dissolved-O2 scavenger config (see caveat below)
./sim beam_boscolo.in     # BoscoloChem reaction network, see "Choosing a chemistry"
./sim reaction_counter.in # reaction-counting example, see below
./sim example_sbs.in      # chemistry mode SBS (particle-based stage only)
./sim example_irt.in      # chemistry mode IRT_syn, no mesoscopic stage
./sim example_meso.in     # chemistry mode IRT_syn + mesoscopic stage (smoke-sized)
```

The three `example_*.in` macros differ only in the chemistry mode; see
"Chemistry modes" below. Each runs two sub-runs of 2 events (10 keV e-,
dissolved O2 at 21 %) into `results/sub_01/` and `results/sub_02/`.

Other flags:

```bash
./sim beam.in --threads 4       # multithreaded run (default: Serial)
./sim beam.in --dir runs/001    # redirect all output files into runs/001
```

`--dir <path>` can appear anywhere on the command line, but the macro
filename must still come first. `<path>` is created if missing; its parent
must already exist.

The macro command `/run/outputDir <path>` (`PreInit` only, before
`/run/initialize`) sets the same output directory from inside a macro
instead of the command line:

```text
/run/outputDir runs/001

/run/initialize
```

`--dir` is always applied before the macro runs, so if a macro sets a
*different* path than `--dir` did, the run aborts with a fatal
configuration error instead of silently picking one. Setting the same path
from both is a harmless no-op.

Without `--dir` or `/run/outputDir`, output goes to `<exeDir>/results`
(e.g. `build/results`). That directory's `Manifest.json` is a results index
listing every dump (`dumps`, one entry per `/run/dumpDataAndReset*`), each
pointing at its own `Manifest.json` in its folder.

## Output files

Each run writes:

- `Species.Txt` — human-readable species yields vs. time
- `Species_nt_species.csv` — aggregate `sumG`/`sumG2` per species/time (nt = NTuple)
- `Species_nt_species_all.csv` — same, per event
- `Reactions.Txt`, `Reactions_nt_reactions.csv`, `ReactionsMetadata.csv` —
  reaction-firing counts (see [Reaction counter](#reaction-counter) below)
- `output_event_<n>.txt` (Serial) or `output_event_t<thread>_e<event>.txt`
  (MT) — per-event pre-chemical dumps

Multiple `/run/beamOn` calls in one macro overwrite `Reactions.Txt` and
produce `*_bis.csv` names for the later ntuples, same as the species files.

## Logging

The project uses a custom `DnaLogger` instead of ad hoc console output:

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

Default is `Error` (quiet). Use `Info` or `Debug` to see progress and
diagnostic messages, e.g. the reaction-counter write confirmation below.

## Chemistry

Species and reactions are defined in code
(`src/chemistry/DnaChemistryList.cc`/`src/chemistry/catalog/PureWaterReactions.cc`), not via macro
commands — `/chem/species` and `/chem/reaction/add` are intentionally not
used (`/chem/reaction/UI` would reset the shared reaction table and wipe the
class-built one).

The pH-driven acid-base buffer network and the full O2⁻/HO2/HO2⁻/O⁻/O3⁻
network are **baseline and always on** — pure water radiolysis alone can
produce O2 through this network. `/chem/env/O2 <percent>` is currently a
no-op reserved for a future *exogenous* dissolved-O2 supply mechanism;
`/chem/env/pH <double>` is active and drives the bulk `H3Op(B)`/`OHm(B)`
buffer concentration. See `CONTEXT.md` and
`docs/adr/0001-baseline-acid-base-buffer.md` for the full vocabulary and
rationale.

### Choosing a chemistry

The reaction content (the reaction table and the acid-base buffer rates) is a
named **Chemistry**. Pick one in the macro, before `/run/initialize`:

```text
/chem/select BoscoloChem
/run/initialize
```

- `PureWater` is the default, so macros without `/chem/select` behave as before.
- Names are case-insensitive. `/chem/list` prints the available ones.
- An unknown name, or two different names in one process, is a fatal error.
  The selection can't change after `/run/initialize`.
- `BoscoloChem` is currently a copy of `PureWater`, marked work in progress
  (`src/chemistry/catalog/BoscoloChemReactions.cc`): edit its reactions there.
- To add a Chemistry, copy `src/chemistry/catalog/PureWaterReactions.cc` and its header, rename
  them, and register the new builders in `src/chemistry/BuiltInChemistries.cc`. A
  Chemistry may leave the acid-base list partial or empty; see
  `docs/adr/0002-named-chemistries.md`.

`macro/beam_boscolo.in` is a ready-to-run example.

## Chemistry modes

Three modes are selectable before `/run/initialize` (ADR 0008):

- **IRT_syn + mesoscopic** (default): particle-based stage up to the
  hand-over time, then the mesoscopic stage. Tuned with `/chem/meso/handOverTime`,
  `/chem/meso/voxelSize`, `/chem/meso/timesPerDecade`. Example:
  `macro/example_meso.in`.
- **IRT_syn only**: `/chem/meso/enable false`. Particle-based stage only.
  Example: `macro/example_irt.in`.
- **SBS**: `/process/chem/TimeStepModel SBS`. Step-by-step Brownian dynamics,
  particle-based stage only, no mesoscopic stage. Example: `macro/example_sbs.in`.
  SBS differs from IRT_syn + mesoscopic in its yields (see
  `docs/irt-syn-mesoscopic-validation.md`); compare modes, do not mix them.

Combining SBS with an explicit `/chem/meso/enable true` is a fatal error.

The default chemistry end time is 1 us without the mesoscopic stage and 1 s
with it. To override it, issue `/scheduler/endTime <value> <unit>` *after*
`/run/initialize` — anything issued before that point is overwritten.

## Reaction counter

`ReactionCounter` (`src/scoring/ReactionCounter.cc`) counts, per time bin, how many
times each bimolecular reaction fires — live, during the run, via
`TimeStepAction::UserReactionAction`. Counts are merged across worker
threads and aggregated over all events in the run. Acid-base/scavenger
reactions never reach that hook, so they are not counted.

### Configuring the time bins

Both commands are `PreInit` (issue them before `/run/initialize`) and are
mutually exclusive — issuing both in the same macro is a fatal
configuration error:

- `/chem/reaction/timeBinsFixed <width> <unit>` — a fixed step, expanded up
  to the chemistry scheduler's end time.
- `/chem/reaction/timeBinsList <e1> <e2> ... <eN> <unit>` — explicit bin
  edges.

If neither is issued, a built-in 7-edge default table is used: 1, 10, 100,
1000, 10000, 100000, 999999 ps.

`/chem/reaction/dump <filename>` writes the full reaction table (bimolecular
+ acid-base networks) to `<filename>` for inspection — also `PreInit`.

### Output format

- `Reactions.Txt` — human-readable, one block per time bin, each reaction
  printed with its full `"A + B -> C + D"` label and firing count.
- `Reactions_nt_reactions.csv` — rows of `(reactionId, time, count)`.
- `ReactionsMetadata.csv` — maps each `reactionId` to its full
  `"A + B -> C + D"` label.

The `[RunAction] reaction counts written...` confirmation line is logged at
`Info` level, so it's silent unless `/dnaLogger/verbose Info` (or more
verbose) is set.

### Example: `macro/reaction_counter.in`

```text
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info

/process/dna/e-SolvationSubType Ritchie1994

# Bin reaction counts at a fixed 100 ps step (PreInit, before /run/initialize).
/chem/reaction/timeBinsFixed 100 picosecond

/run/initialize
/scheduler/endTime 1 us

/gun/particle e-
/gun/energy 100 keV

/run/beamOn 4
```

Run it with:

```bash
./sim reaction_counter.in
```

This produces `Reactions.Txt`, `Reactions_nt_reactions.csv`, and
`ReactionsMetadata.csv` with counts binned every 100 ps out to the 1 µs
scheduler end time the macro sets, plus the usual `Species.*` and
`SpeciesMeso.*` output. Reactions are only counted in the particle-based
stage, so every bin after the hand-over time (5 ns by default) is empty.

To use explicit bin edges instead, comment out `timeBinsFixed` and use, e.g.:

```text
/chem/reaction/timeBinsList 1 10 100 1000 picosecond
```

## Testing

Unit tests (`test/*Test.cc`, plain `assert` + CTest) are built and run from
`build-ninja/` (Debug). See `.claude/geant4-instructions.md` section 5 for
the full procedure, including `NDEBUG` and Debug-CRT-dialog pitfalls.

## Repo layout

- `sim.cc` — application entry point, CLI flags (`--threads`, `--dir`) and
  macro-command messengers (`--dir`'s counterpart, `/run/outputDir`, lives
  in `src/core/OutputDirMessenger.cc`)
- `src/`, `header/` — implementation and headers, mirrored in clusters: `core/` (`ArgParser`, `OutputDir`, `DnaLogger`), `actions/` (run, event, tracking, stacking and stepping actions), `geometry/` (`DetectorConstruction`, `DnaChemistryWorld`), `physics/` (`PhysicsList`), `chemistry/` (`DnaChemistryList`, `ChemistryRegistry`, `TimeStepAction`) with `chemistry/catalog/` (`PureWaterReactions`, `BoscoloChemReactions`), and `scoring/` (`ScoreSpecies`, `ReactionCounter`, `RunAccumulator`). Project includes are rooted at `header/`, e.g. `#include "chemistry/DnaChemistryList.hh"`.
- `macro/` — runtime beam and chemistry configuration macros
- `test/` — unit tests (CTest, no Geant4 kernel dependency)
- `docs/adr/` — architecture decision records
- `CONTEXT.md` — chemistry terminology glossary
- `CLAUDE.md` — full project conventions and agent instructions

## Further reading

- `CONTEXT.md` — chemistry vocabulary (Chemistry, pure-water chemistry,
  scavenger, bulk species)
- `docs/adr/0001-baseline-acid-base-buffer.md` — why the acid-base/O2
  network is baseline rather than opt-in
- `docs/adr/0002-named-chemistries.md` — why Chemistries are named and
  selectable, and why the acid-base list can be empty
- `CLAUDE.md` — full build/run/test procedure references, coding
  conventions, and agent workflow
