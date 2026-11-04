<p align="center">
  <img src="docs/assets/dnachem-min-icon.svg" alt="dnachem-min icon: an electron track ionizing a water molecule" width="120">
</p>

<h1 align="center">dnachem-min</h1>

<p align="center">
Minimal Geant4-DNA application for water radiolysis from electron beams:<br>
physics, chemistry and scoring for conventional and ultra-high-dose-rate (UHDR) studies.
</p>

<p align="center">
  <a href="https://rtonneau.github.io/dnachem-min/explorer/"><img alt="Architecture explorer" src="https://img.shields.io/badge/docs-architecture_explorer-0a7ea4?logo=githubpages&logoColor=white"></a>
  <a href="https://rtonneau.github.io/dnachem-min/explorer/#builder"><img alt="Macro builder" src="https://img.shields.io/badge/tool-macro_builder-0a7ea4?logo=githubpages&logoColor=white"></a>
  <a href="https://github.com/rtonneau/dnachem-min/actions/workflows/docs-checks.yml"><img alt="docs-checks" src="https://github.com/rtonneau/dnachem-min/actions/workflows/docs-checks.yml/badge.svg"></a>
  <a href="https://github.com/rtonneau/dnachem-min/tags"><img alt="Release" src="https://img.shields.io/github/v/tag/rtonneau/dnachem-min?sort=semver&label=release"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
  <br>
  <a href="https://geant4.web.cern.ch/"><img alt="Geant4-DNA 11.4.1" src="https://img.shields.io/badge/Geant4--DNA-11.4.1-1f6feb"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake 3.16+" src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white">
  <img alt="HDF5 C++" src="https://img.shields.io/badge/HDF5-C%2B%2B-2e6db4">
  <img alt="Verified on Windows / MSVC" src="https://img.shields.io/badge/verified-Windows_%2F_MSVC-555">
  <br>
  <img alt="Chemistries: 4" src="https://img.shields.io/badge/chemistries-4-8a3ffc">
  <img alt="Chemistry modes" src="https://img.shields.io/badge/modes-SBS_%C2%B7_IRT__syn_%C2%B7_IRT__syn_%2B_meso-8a3ffc">
  <img alt="Last commit" src="https://img.shields.io/github/last-commit/rtonneau/dnachem-min">
</p>

---

UHDR is a use case, not a separate implementation: there is no pulse-structure,
dose-rate-effect or delivery-model code. The geometry is one homogeneous water
box (no voxelization).

## Highlights

- **Three chemistry modes**: SBS, IRT_syn, and IRT_syn handing over to a
  mesoscopic (Gillespie, cell-mesh) stage up to 1 s (the default).
- **Four named Chemistries** selectable from a macro, from pure water to
  published networks (see [Chemistry](#chemistry)).
- **Dissolved O2 and pH** set from the macro (`/chem/env/scavenger`, `/chem/env/pH`).
- **Structured output**: data accumulates across `/run/beamOn` calls and is
  written by a dump, each with a `Manifest.json` (beam, seed, energy deposit,
  Chemistry, environment, files) and a results index.
- **Optional spatial output**: per-event mesoscopic snapshots in HDF5.
- **[Architecture explorer](https://rtonneau.github.io/dnachem-min/explorer/)**:
  module map, run flow, task guides and a macro builder, in one offline HTML page
  (`docs/explorer/index.html`).

## Requirements

- Geant4 with DNA models (verified with 11.4.1; older versions are not verified)
- CMake 3.16+
- A C++20 compiler (verified with MSVC on Windows; GCC 13.3 on Linux, Ubuntu 24.04 with Geant4 11.4.3; clang not verified)
- HDF5 with C++ support, optional: needed only for `/chem/meso/spatialOutput` (`SpeciesMesoSpatial.h5`)

## Build

The full MSVC procedure (the `build/` vs `build-ninja/` split, background runs,
verification checklist) is in [`.claude/geant4-instructions.md`](.claude/geant4-instructions.md).
In short: `build/` is a RelWithDebInfo tree used to run `sim`; `build-ninja/` is a
Debug tree used to run the unit tests with CTest. The build copies `macro/` next to
the executable.

### Linux (verified on Ubuntu 24.04 (WSL 2), GCC 13.3, Geant4 11.4.3)

Packages (Ubuntu/Debian; `libhdf5-dev` only for HDF5 support):

```bash
sudo apt install build-essential cmake ninja-build libexpat1-dev libxerces-c-dev libhdf5-dev
```

As on Windows, one RelWithDebInfo tree to run `sim` and a separate Debug tree for
the tests: the tests use plain `assert`, which `NDEBUG` compiles out in
RelWithDebInfo and Release, so they would pass without checking anything there.

```bash
source /path/to/geant4-install/bin/geant4.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo [-DDNACHEM_WITH_HDF5=OFF|ON]
cmake --build build -j
(cd build && ./sim beam.in)
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j && ctest --test-dir build-debug
```

WSL: by default WSL appends the Windows `PATH`, so CMake can pick up a Windows HDF5
(e.g. vcpkg's), which fails to compile (`ssize_t` conflict in `H5public.h`). Set
`appendWindowsPath=false` under `[interop]` in `/etc/wsl.conf` and restart WSL
(`wsl --shutdown`).

`DNACHEM_WITH_HDF5` is `AUTO` by default: HDF5 is used if CMake finds it. `ON`
requires it (configure fails otherwise); `OFF` builds without it. Without HDF5,
`/chem/meso/spatialOutput true` is a fatal error and `Manifest.json` has
`"hdf5": false`. Note that `find_package(Geant4)` itself pulls in HDF5 when Geant4
was built with HDF5 analysis support (`GEANT4_USE_HDF5=ON`). dnachem-min's own code
then no longer needs HDF5, but a fully HDF5-free build also needs a Geant4 built
without HDF5 (`GEANT4_USE_HDF5=OFF`, the Geant4 default).

## Quick start

From the run build directory, pass the macro **filename only**. `sim` looks for it
as given, then in `<exeDir>/`, then in `<exeDir>/macro/`.

```bash
./sim example_meso.in   # IRT_syn + mesoscopic stage (default mode, smoke-sized)
./sim example_irt.in    # IRT_syn only
./sim example_sbs.in    # SBS
./sim beam.in           # pure water, default Chemistry
./sim beam_o2.in        # dissolved O2 at 21 %
```

The three `example_*.in` macros differ only in the chemistry mode: 10 keV e-,
dissolved O2 at 21 %, two sub-runs of 2 events dumped to `results/sub_01/` and
`results/sub_02/`.

```bash
./sim beam.in --threads 4       # multithreaded (default: Serial)
./sim beam.in --dir runs/001    # write all output into runs/001
```

`/run/outputDir <path>` (before `/run/initialize`) is the macro counterpart of
`--dir`; setting two different paths is a fatal error. Without either, output goes
to `<exeDir>/results`.

To write a macro without memorising commands, use the
[macro builder](https://rtonneau.github.io/dnachem-min/explorer/#builder).

## Chemistry

Molecules and reactions are defined in code, not with `/chem/species` or
`/chem/reaction/add` (`/chem/reaction/UI` would wipe the class-built reaction table).

### Choosing a Chemistry

Select one before `/run/initialize` (case-insensitive; `/chem/list` prints them):

```text
/chem/select Tonneau2025
/run/initialize
```

| Chemistry | Content | Source |
|---|---|---|
| `PureWater` (default) | 9 base reactions + O2-derived network, acid-base buffer driven by pH | Geant4-DNA UHDR example |
| `BoscoloChem` | 26 reactions; no acid-base buffer (pH has no effect) | Boscolo et al., Int. J. Mol. Sci. 21 (2020) 424, Table 1 |
| `Tonneau2025` | 73 reactions, adds the HO3 molecule | Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, Table 2 |
| `Farokhi2023` | 9 base-water reactions; no acid-base buffer | Farokhi et al. 2023, via Geant4 `dna/scavenger/Rtable_O2.txt` |

To add one, follow [`docs/recipes/add-a-new-chemistry.md`](docs/recipes/add-a-new-chemistry.md)
(design: [ADR 0002](docs/adr/0002-named-chemistries.md)).

### Chemistry modes

Chosen before `/run/initialize` ([ADR 0008](docs/adr/0008-selectable-chemistry-modes.md)):

| Mode | Commands | Default end time |
|---|---|---|
| IRT_syn + mesoscopic (default) | none | 1 s |
| IRT_syn only | `/chem/meso/enable false` | 1 us |
| SBS | `/process/chem/TimeStepModel SBS` | 1 us |

SBS with an explicit `/chem/meso/enable true` is fatal. The mesoscopic stage is
tuned with `/chem/meso/handOverTime` (default 5 ns), `/chem/meso/voxelSize`,
`/chem/meso/timesPerDecade` and `/chem/meso/spatialOutput`. Override the end time
with `/scheduler/endTime <value> <unit>` **after** `/run/initialize`. The modes give
different yields ([validation](docs/irt-syn-mesoscopic-validation.md)); compare
them, don't mix them.

### Environment

All before `/run/initialize`:

- `/chem/env/scavenger O2 21 %`: dissolved O2 (`M`, `mM`, `uM`, or `%` for O2 with
  kH = 0.0013 M; 0 = absent), reacting through the Chemistry's bulk reactions
  ([ADR 0004](docs/adr/0004-scavenger-reactions-per-chemistry.md)).
- `/chem/env/pH 7`: drives the bulk H3O+/OH- buffer of Chemistries that have one.
- `/chem/env/halfBox 500 um`: half side of the water box (the whole geometry).

## Output

Nothing is written during `/run/beamOn` except staging files. Data accumulates and
is written by a **dump**: `/run/dumpDataAndReset [prefix]` or
`/run/dumpDataAndResetToDir <subdir>`; if the macro never dumps, a final flush with
prefix `EndOfRun_` runs at exit.

| File | Content |
|---|---|
| `Manifest.json` | dump metadata; the one in the output root also indexes every dump |
| `Species.Txt`, `Species_nt_species.csv` | species yields vs. time, particle-based stage |
| `SpeciesMeso.Txt`, `SpeciesMeso.csv` | mean count per event, mesoscopic stage |
| `SpeciesMesoSpatial.h5` | per-event mesh snapshots (`/chem/meso/spatialOutput true`) |
| `Reactions.Txt`, `Reactions_nt_reactions.csv`, `ReactionsMetadata.csv` | reaction firings per time bin, particle-based stage |
| `PhysicsInteractions.Txt`, `PhysicsInteractions.csv` | counts of G4DNA physics processes |
| `TrackLengths.Txt`, `TrackLengths.csv` | primary and secondary-electron track lengths per event |
| `PreChemical_run<R>_event<E>.txt` | pre-chemical species per event |

Layouts and examples: [`docs/output/README.md`](docs/output/README.md); HDF5 format:
[`docs/output/SpeciesMesoSpatial-h5.md`](docs/output/SpeciesMesoSpatial-h5.md).

### Reaction time bins

Before `/run/initialize`, pick one (both together is fatal):

- `/chem/reaction/timeBinsFixed 100 ps`: fixed width up to the end time;
- `/chem/reaction/timeBinsList 1 10 100 1000 ps`: explicit edges.

Default edges: 1, 10, 100, 1000, 10000, 100000, 999999 ps. Example:
`macro/reaction_counter.in`.

## Logging

```text
/dnaLogger/verbose Quiet|Error|Warning|Info|Debug|Trace
```

The default is `Error`. Use `Info` or `Debug` for progress and diagnostics.

## Testing

Unit tests (`test/*Test.cc`, plain `assert` + CTest) are built and run from
`build-ninja/` (Debug); see section 5 of
[`.claude/geant4-instructions.md`](.claude/geant4-instructions.md). On GitHub, the
`docs-checks` workflow validates the architecture explorer and the macro-builder
model (there is no Geant4 build in CI).

## Repository layout

| Path | Content |
|---|---|
| `sim.cc` | entry point, CLI flags |
| `src/`, `header/` | code in clusters: `core/`, `actions/`, `geometry/`, `physics/`, `chemistry/` (+ `chemistry/catalog/` for Chemistries), `scoring/`; includes rooted at `header/` |
| `macro/` | example and study macros |
| `test/` | unit tests (no Geant4 kernel) |
| `docs/explorer/` | architecture explorer and macro builder |
| `docs/recipes/` | how-to guides for recurring development tasks |
| `docs/adr/` | architecture decision records |
| `docs/output/` | output layouts and formats |
| `CONTEXT.md` | chemistry vocabulary |
| `CHANGELOG.md` | release notes |
| `CLAUDE.md` | full project conventions |

## Citing

If you use dnachem-min, please cite:

- R. Tonneau et al., Phys. Med. Biol. 70 (2025) 235021.

Geant4-DNA asks users to cite the collaboration publications:

- S. Incerti et al., Med. Phys. 45 (2018) e722–e739.
- M. A. Bernal et al., Phys. Med. 31 (2015) 861–874.
- S. Incerti et al., Med. Phys. 37 (2010) 4692–4708.
- S. Incerti et al., Int. J. Model. Simul. Sci. Comput. 1 (2010) 157–178.

and Geant4 itself:

- S. Agostinelli et al., Nucl. Instrum. Meth. A 506 (2003) 250–303.
- J. Allison et al., IEEE Trans. Nucl. Sci. 53 (2006) 270–278.
- J. Allison et al., Nucl. Instrum. Meth. A 835 (2016) 186–225.

## License

[MIT](LICENSE).
