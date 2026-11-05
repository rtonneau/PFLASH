<p align="center">
  <img src="docs/assets/dnachem-min-icon.svg" alt="dnachem-min icon: an electron track ionizing a water molecule" width="120">
</p>

<h1 align="center">dnachem-min</h1>

<p align="center">
Minimal Geant4-DNA application for water radiolysis from electron beams:<br>
physics, chemistry and scoring for conventional-dose-rate (CDR) studies.
</p>

<p align="center">
  <a href="https://rtonneau.github.io/dnachem-min/explorer/"><img alt="Architecture explorer" src="https://img.shields.io/badge/docs-architecture_explorer-0a7ea4?logo=githubpages&logoColor=white"></a>
  <a href="https://rtonneau.github.io/dnachem-min/explorer/#builder"><img alt="Macro builder" src="https://img.shields.io/badge/tool-macro_builder-0a7ea4?logo=githubpages&logoColor=white"></a>
  <a href="https://github.com/rtonneau/dnachem-min/actions/workflows/docs-checks.yml"><img alt="docs-checks" src="https://github.com/rtonneau/dnachem-min/actions/workflows/docs-checks.yml/badge.svg"></a>
  <a href="https://github.com/rtonneau/dnachem-min/tags"><img alt="Release" src="https://img.shields.io/github/v/tag/rtonneau/dnachem-min?sort=semver&label=release"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
  <br>
  <a href="https://geant4.web.cern.ch/"><img alt="Geant4-DNA 11.4.3" src="https://img.shields.io/badge/Geant4--DNA-11.4.3-1f6feb"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake 3.16+" src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white">
  <img alt="HDF5 C++" src="https://img.shields.io/badge/HDF5-C%2B%2B-2e6db4">
  <img alt="Verified on Windows / MSVC" src="https://img.shields.io/badge/verified-Windows_%2F_MSVC-555">
  <img alt="Verified on Linux / GCC" src="https://img.shields.io/badge/verified-Linux_%2F_GCC-555">
  <br>
  <img alt="Chemistries: 4" src="https://img.shields.io/badge/chemistries-4-8a3ffc">
  <img alt="Chemistry modes" src="https://img.shields.io/badge/modes-SBS_%C2%B7_IRT__syn_%C2%B7_IRT__syn_%2B_meso-8a3ffc">
  <img alt="Last commit" src="https://img.shields.io/github/last-commit/rtonneau/dnachem-min">
</p>

> **Explore online (GitHub Pages)**
> - [Architecture explorer](https://rtonneau.github.io/dnachem-min/explorer/): module map, run flow, task guides, reaction catalogue.
> - [Macro builder](https://rtonneau.github.io/dnachem-min/explorer/#builder): write a macro without memorising commands.
> - [Prebuilt Windows package](https://github.com/rtonneau/dnachem-min/releases/latest): `sim.exe` + `macro/` (you supply Geant4).
> - Source of these pages: [`docs/explorer/`](docs/explorer/).

---

## What it is

This is the conventional-dose-rate (CDR) base case only. Ultra-high-dose-rate
(UHDR) studies are meant to be built on top of it later: there is no
pulse-structure, dose-rate-effect or delivery-model code. The geometry is one
homogeneous water box (no voxelization).

## Highlights

- **Three chemistry modes**: SBS, IRT_syn, and IRT_syn handing over to a
  mesoscopic (Gillespie, cell-mesh) stage up to 1 s (the default).
- **Four named Chemistries** selectable from a macro, from pure water to
  published networks (see [Chemistry](#chemistry)).
- **Dissolved O2 and pH** set from the macro (`/chem/env/scavenger`, `/chem/env/pH`).
- **Structured output**: data accumulates across `/run/beamOn` calls and is
  written by a dump, each with a `Manifest.json` (beam, seed, energy deposit,
  Chemistry, environment, files) and a results index.
- **Selectable output formats**: every output product can be written as `txt`,
  `csv`, `h5` and/or `root` (`/output/<product>/format`, default `csv`), with one
  table schema per product whatever the format. `root` needs no ROOT install.
- **Optional spatial output**: per-event mesoscopic snapshots in HDF5 or ROOT.
- **[Architecture explorer](https://rtonneau.github.io/dnachem-min/explorer/)**:
  module map, run flow, task guides and a macro builder, in one offline HTML page
  (`docs/explorer/index.html`).

## Glossary

Full definitions: [`CONTEXT.md`](CONTEXT.md).

| Term | Meaning | See |
|---|---|---|
| **CDR / UHDR** | Conventional / ultra-high dose rate. This project is the CDR base case; UHDR is future work. | [What it is](#what-it-is) |
| **Chemistry** | A named, selectable reaction network (`PureWater`, `BoscoloChem`, `Tonneau2025`, `Farokhi2023`). | [Choosing a Chemistry](#choosing-a-chemistry) |
| **Chemistry mode** | How an event's chemistry runs: SBS, IRT_syn, or IRT_syn + mesoscopic (default). | [Chemistry modes](#chemistry-modes) |
| **SBS / IRT_syn** | Step-by-step and synchronous independent-reaction-times schemes for the particle-based stage. | [Chemistry modes](#chemistry-modes) |
| **Particle-based stage** | Chemistry where each molecule is an individual particle, up to the hand-over time. | [Chemistry modes](#chemistry-modes) |
| **Mesoscopic stage** | Chemistry on molecule counts per cell of a mesh, after the hand-over; reaches up to seconds. | [Chemistry modes](#chemistry-modes) |
| **Hand-over time** | Moment the particle-based stage gives way to the mesoscopic stage (default 5 ns). | [Chemistry modes](#chemistry-modes) |
| **Scavenger** | Dissolved species such as O2, set with `/chem/env/scavenger`. | [Environment](#environment) |
| **Bulk reaction** | Reaction of a tracked molecule with a bulk species (acid-base buffer, scavenger). | [Environment](#environment) |
| **Extra molecule** | Molecule that exists only under one Chemistry (HO3 under `Tonneau2025`). | [Choosing a Chemistry](#choosing-a-chemistry) |
| **Dump** | One flush of accumulated results to files, then a reset. | [Output](#output) |
| **Output product / format** | A kind of result (`Species`, `Reactions`, ...) and its encoding (`txt`, `csv`, `h5`, `root`). | [Output](#output) |
| **Manifest** | `Manifest.json` describing one dump. | [Output](#output) |
| **Spatial snapshot** | Opt-in per-event picture of the mesoscopic mesh. | [Output](#output) |

## Contents

[Quick start](#quick-start) · [Installation](#installation) · [Chemistry](#chemistry) · [Output](#output) · [Reaction time bins](#reaction-time-bins) · [Logging](#logging) · [Development](#development) · [Citing](#citing)

## Quick start

Needs a built `sim` (see [Installation](#installation)) or the prebuilt Windows package.

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

## Installation

Skip this if you use the prebuilt Windows package (see Windows below).

### Requirements

- Geant4 with DNA models (verified with 11.4.3; older versions are not verified)
- CMake 3.16+
- A C++20 compiler (verified with MSVC on Windows; GCC 13.3 on Linux, Ubuntu 24.04 with Geant4 11.4.3; clang not verified)
- HDF5 with C++ support, optional: needed only for the `h5` output format


There are two build trees, on both platforms, on purpose:

- a **run tree** (RelWithDebInfo) to run `sim`;
- a **test tree** (Debug) to run the unit tests with CTest. The tests use plain
  `assert`, which `NDEBUG` compiles out in RelWithDebInfo and Release, so they
  would pass there without checking anything.

The build copies `macro/` next to the executable, so `sim` finds the example
macros by filename.

### HDF5 option (both platforms)

`DNACHEM_WITH_HDF5` is `AUTO` by default: HDF5 is used if CMake finds it. `ON`
requires it (configure fails otherwise); `OFF` builds without it. The configure
log says which form it used (`-- HDF5: using ...`). Without HDF5, choosing the
`h5` format for any product (`/output/<product>/format h5`, or in
`/output/defaultFormat`) is a fatal error and `Manifest.json` has
`"hdf5": false`; everything else works, including `root`.
Note that `find_package(Geant4)` itself pulls in HDF5 when Geant4 was built
with HDF5 analysis support
(`GEANT4_USE_HDF5=ON`). dnachem-min's own code then no longer needs HDF5, but a
fully HDF5-free build also needs a Geant4 built without HDF5
(`GEANT4_USE_HDF5=OFF`, the Geant4 default).

### Windows (verified: Windows 11, MSVC, Geant4 11.4.3, vcpkg HDF5 1.14.6)

<details><summary>Windows steps</summary>

No build setup? A prebuilt `dnachem-min-win64.zip` (`sim.exe`, the HDF5 DLLs and
`macro/`) is attached to each [GitHub Release](https://github.com/rtonneau/dnachem-min/releases/latest).
It still needs your own Geant4 install, of the same version and build options;
see `packaging/README.txt` in the zip and [`docs/release.md`](docs/release.md).

**1. Tools.** Install Visual Studio 2022 or later (Community is enough) with the
*Desktop development with C++* workload. It provides MSVC, CMake and Ninja
(*C++ CMake tools for Windows* component). Git is needed to clone the repository.

**2. Geant4.** Build and install Geant4 from source with MSVC x64, shared
libraries, multithreading and the datasets, for example (from the
*x64 Native Tools Command Prompt*, paths to adapt):

```bat
cmake -S geant4-v11.4.3 -B geant4-build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_INSTALL_PREFIX=C:/path/to/geant4-v11.4.3-install ^
      -DGEANT4_BUILD_MULTITHREADED=ON -DGEANT4_INSTALL_DATA=ON
cmake --build geant4-build
cmake --install geant4-build
```

Add Qt/OpenGL options (`-DGEANT4_USE_QT=ON`, ...) only if you want the GUI; a
Geant4 built with Qt then needs Qt's `bin\` on `PATH` when `sim.exe` runs.

**3. HDF5 (optional).** Install it with [vcpkg](https://github.com/microsoft/vcpkg)
including the C++ library:

```powershell
git clone https://github.com/microsoft/vcpkg C:/path/to/vcpkg
C:/path/to/vcpkg/bootstrap-vcpkg.bat
C:/path/to/vcpkg/vcpkg install "hdf5[cpp]:x64-windows"
```

Pass vcpkg's toolchain file at configure (below); it lets CMake find HDF5 and
copies `hdf5.dll` / `hdf5_cpp.dll` next to `sim.exe`. Without the toolchain file
`AUTO` simply builds without HDF5.

**4. Configure and build.** Compiling needs the MSVC x64 environment (otherwise
it fails on missing STL headers such as `cstddef`). Open the
*x64 Native Tools Command Prompt for VS*, or load `vcvars64.bat` in PowerShell:

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && <command>"
```

Then, from the repository root (`[-DDNACHEM_WITH_HDF5=OFF|ON]` is optional):

```powershell
# run tree
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
      -DGeant4_DIR=C:/path/to/geant4-v11.4.3-install/lib/cmake/Geant4 `
      -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --target sim

# test tree
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DGeant4_DIR=C:/path/to/geant4-v11.4.3-install/lib/cmake/Geant4 `
      -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-ninja
```

**5. Run.** From PowerShell (not Git Bash), in `build\`, with Geant4's `bin\`
(and Qt's, if Geant4 uses Qt) on `PATH`:

```powershell
$env:PATH = "C:/path/to/geant4-v11.4.3-install/bin;$env:PATH"
cd build
.\sim.exe example_irt.in
```

If `sim.exe` exits at once with code `0xC0000135`, a DLL was not found
(typically Geant4's or Qt's): add its directory to `PATH`.

**6. Test.** The Debug tests need vcpkg's *Debug* HDF5 DLLs first on `PATH`
(with the release DLLs first, `MesoSpatialFileTest` can hang):

```powershell
$env:PATH = "C:/path/to/vcpkg/installed/x64-windows/debug/bin;$env:PATH"
ctest --test-dir build-ninja --output-on-failure
```

Every test must report `Passed`; `Not Run` means its executable was not built.
The Node-based checks (explorer, macro builder, `CatalogExportCheck`) run only
when `node` is on `PATH`; `CatalogExportCheck` uses `build\sim.exe`, so build the
run tree first. More detail (background runs, verification checklist):
[`.claude/geant4-instructions.md`](.claude/geant4-instructions.md).

</details>

### Linux (verified: Ubuntu 24.04 (WSL 2), GCC 13.3, Geant4 11.4.3, HDF5 1.10.10)

<details><summary>Linux and WSL steps</summary>

**1. Packages** (Ubuntu/Debian; `libhdf5-dev` only for HDF5 support, it
provides the C++ library too):

```bash
sudo apt install build-essential cmake ninja-build git \
                 libexpat1-dev libxerces-c-dev libhdf5-dev
```

`libexpat1-dev` and `libxerces-c-dev` are for the Geant4 build below (system
Expat, GDML); drop them if your Geant4 is configured without those options.

**2. Geant4.** Build and install it from source, for example (the verified
setup; Qt is not needed, `sim` runs in batch mode):

```bash
cmake -S geant4-v11.4.3 -B geant4-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/geant4/install \
      -DGEANT4_BUILD_MULTITHREADED=ON -DGEANT4_INSTALL_DATA=ON \
      -DGEANT4_USE_GDML=ON -DGEANT4_USE_SYSTEM_EXPAT=ON
cmake --build geant4-build -j
cmake --install geant4-build
```

Source `geant4.sh` in every shell that builds or runs `sim`: it puts Geant4 on
CMake's search path and on `LD_LIBRARY_PATH`, and sets the dataset variables
(`G4LEDATA`, ...).

```bash
source $HOME/geant4/install/bin/geant4.sh
```

**3. Configure and build**, from the repository root (`[-DDNACHEM_WITH_HDF5=OFF|ON]`
is optional; the distro HDF5 is found by CMake's FindHDF5 module):

```bash
# run tree
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# test tree
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```

**4. Run**, from the run tree:

```bash
cd build && ./sim example_irt.in
```

**5. Test:**

```bash
ctest --test-dir build-debug --output-on-failure
```

Every test must report `Passed`. As on Windows, the Node-based checks run only
when `node` is installed, and `CatalogExportCheck` uses `build/sim` (build the
run tree first, or set `-DCATALOG_EXPORT_SIM=<path>`).

**WSL.** By default WSL appends the Windows `PATH`, so CMake can pick up a Windows
HDF5 (e.g. vcpkg's), which fails to compile (`ssize_t` conflict in
`H5public.h`). Set `appendWindowsPath=false` under `[interop]` in
`/etc/wsl.conf` and restart WSL (`wsl --shutdown`). Clone the repository into the
Linux file system (e.g. `~/dnachem-min`), not under `/mnt/c`: builds there are
much slower.

</details>

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
`/chem/meso/timesPerDecade`; spatial snapshots are chosen with
`/output/speciesMesoSpatial/format` (see [Output](#output)). Override the end time
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

Each kind of data is an **output product**, written as `<Product>.<ext>` in
the formats chosen for it ([ADR 0009](docs/adr/0009-standardized-output-products.md)).
All format commands are PreInit (before `/run/initialize`) and take one or more of
`txt`, `csv`, `h5`, `root`, or `none`:

```text
/output/defaultFormat csv h5            # every product without its own command (built-in: csv)
/output/reactions/format txt root       # one product, overrides the default
/output/preChemical/format none         # do not write this product
/output/speciesMesoSpatial/format h5    # h5/root only; off unless set, ignores the default
```

| Product (`/output/<name>/format`) | Files | Content |
|---|---|---|
| `Species` (`species`) | `Species.<ext>` | species count and G value vs. time (sum, mean, std. error per event), particle-based stage |
| `SpeciesMeso` (`speciesMeso`) | `SpeciesMeso.<ext>` | same per-event statistics, mesoscopic stage |
| `Reactions` (`reactions`) | `Reactions.<ext>`; `ReactionsCatalog.<ext>` in txt/csv (a second table in h5/root) | reaction firings per time bin, particle-based stage, and the reaction id → label catalogue |
| `PhysicsInteractions` (`physicsInteractions`) | `PhysicsInteractions.<ext>` | per-event counts of G4DNA physics processes |
| `TrackLengths` (`trackLengths`) | `TrackLengths.<ext>` | primary and secondary-electron track lengths, one row per event |
| `PreChemical` (`preChemical`) | `PreChemical_run<R>_event<E>.<ext>` in txt/csv; one `PreChemical.<ext>` per dump in h5/root | pre-chemical species per event |
| `SpeciesMesoSpatial` (`speciesMesoSpatial`) | `SpeciesMesoSpatial.h5` / `.root` | per-event mesh snapshots (opt-in, can be large) |

Every dump also writes `Manifest.json` (always JSON, not a product): dump
metadata and the formats used; the one in the output root also indexes every dump.
With the defaults a dump holds the six table products in `csv` and no spatial file.
Column names and units: the `sim-output` skill (`.claude/skills/sim-output/SKILL.md`).

Layouts and examples: [`docs/output/README.md`](docs/output/README.md); spatial
format: [`docs/output/SpeciesMesoSpatial-h5.md`](docs/output/SpeciesMesoSpatial-h5.md);
h5/root table layout: [`docs/output/tables-h5-root.md`](docs/output/tables-h5-root.md).

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
## Development

### Testing

Unit tests (`test/*Test.cc`, plain `assert` + CTest) are built and run from
the Debug test tree (`build-ninja/` on Windows, `build-debug/` on Linux, see
[Install](#install)); see section 5 of
[`.claude/geant4-instructions.md`](.claude/geant4-instructions.md). On GitHub, the
`docs-checks` workflow validates the architecture explorer and the macro-builder
model (there is no Geant4 build in CI).

### Repository layout

| Path | Content |
|---|---|
| `sim.cc` | entry point, CLI flags |
| `src/`, `header/` | code in clusters: `core/`, `actions/`, `geometry/`, `physics/`, `chemistry/` (+ `chemistry/catalog/` for Chemistries), `scoring/`; includes rooted at `header/` |
| `macro/` | example and study macros |
| `test/` | unit tests (no Geant4 kernel) |
| `docs/explorer/` | architecture explorer and macro builder ([online](https://rtonneau.github.io/dnachem-min/explorer/)) |
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
