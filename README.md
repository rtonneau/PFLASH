<p align="center">
  <img src="docs/assets/pflash-icon.svg" alt="PFLASH icon: a pulsed beam sending several tracks into water" width="120">
</p>

<h1 align="center">PFLASH</h1>

<p align="center">
Geant4-DNA water radiolysis, from single tracks towards ultra-high dose rate (FLASH).
</p>

<p align="center">
  <a href="https://geant4.web.cern.ch/"><img alt="Geant4-DNA 11.4.3" src="https://img.shields.io/badge/Geant4--DNA-11.4.3-1f6feb"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white">
  <img alt="Status: research in progress" src="https://img.shields.io/badge/status-research_in_progress-ffcc33">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

## What it is

PFLASH continues [dnachem-min](https://github.com/rtonneau/dnachem-min), a
minimal Geant4-DNA application for water radiolysis from electron beams, and
uses it as the base for ultra-high-dose-rate (UHDR, FLASH) studies.

It has two parts:

- **The base application (`sim`)**: one water box, Geant4-DNA physics, and
  chemistry with SBS, IRT_syn, or IRT_syn followed by a mesoscopic stage up to 1 s.
  Each event is one independent track. Dissolved O2, pH and four published
  reaction networks are set from the macro.
- **Dose-rate work**: tracks that arrive at different times share one chemistry
  history, so their radicals can react with each other. A standalone example is in
  [`examples/poisson-track-irt/`](examples/poisson-track-irt/) (needs a patched
  Geant4); studies and results are in [`docs/dose-rate/`](docs/dose-rate/).

This is research in progress. The dose-rate example checks that its bookkeeping
is correct; it does not yet show a dose-rate effect, and it is not part of `sim`.

## Quick start

Build (details for Windows and Linux in [`docs/install.md`](docs/install.md)):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=<geant4-install>/lib/cmake/Geant4
cmake --build build --target sim
```

Run from the build directory, with the macro filename only:

```bash
./sim example_meso.in            # IRT_syn + mesoscopic stage (default)
./sim example_irt.in             # IRT_syn only
./sim example_sbs.in             # SBS
./sim beam_o2.in --threads 4     # dissolved O2 at 21 %, multithreaded
```

Output goes to `<exeDir>/results` (or `--dir <path>`), with a `Manifest.json`
describing each dump.

## Main macro commands

All before `/run/initialize`:

| Command | Effect |
|---|---|
| `/chem/select PureWater\|BoscoloChem\|Tonneau2025\|Farokhi2023` | reaction network (default `PureWater`) |
| `/process/chem/TimeStepModel SBS` | SBS instead of IRT_syn |
| `/chem/meso/enable false` | no mesoscopic stage (end time 1 us instead of 1 s) |
| `/chem/env/scavenger O2 21 %` | dissolved O2 |
| `/chem/env/pH 7` | acid-base buffer (for Chemistries that have one) |
| `/output/defaultFormat csv h5` | output formats (`txt`, `csv`, `h5`, `root`) |

After `/run/initialize`: `/scheduler/endTime 1 ms` to shorten the chemistry.
Dump results with `/run/dumpDataAndReset [prefix]` or
`/run/dumpDataAndResetToDir <subdir>`.

## Documentation

| Topic | Where |
|---|---|
| Installation | [`docs/install.md`](docs/install.md) |
| Dose-rate studies | [`docs/dose-rate/`](docs/dose-rate/) |
| Output files and formats | [`docs/output/README.md`](docs/output/README.md) |
| Design decisions | [`docs/adr/`](docs/adr/) |
| How-to guides | [`docs/recipes/`](docs/recipes/) |
| Vocabulary | [`CONTEXT.md`](CONTEXT.md) |
| Architecture explorer and macro builder | [`docs/explorer/index.html`](docs/explorer/index.html) |
| Full project conventions | [`CLAUDE.md`](CLAUDE.md) |

## Citing

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
