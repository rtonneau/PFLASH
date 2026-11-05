# Changelog

All notable changes to this project. Versions follow [semantic versioning](https://semver.org/).

## 1.3.0 (2026-11-05)

- Add seeded demonstration macros and setup instructions for notebook analysis, including mesoscopic spatial output.
- Add an executed vertical-slice notebook for inspecting physical interactions, pre-chemical records, particle-stage species and reactions, and mesoscopic counts from configurable dumps.
- Document regenerated Geant4 patch-03 data, the GEANT4 Python environment, normalization checks, and notebook reproduction steps.
- Add a G4Utils work report documenting notebook-driven discovery, reader, and G-value improvements.
- Linux: building with HDF5 (`DNACHEM_WITH_HDF5=ON`, or `AUTO` when HDF5 is found) no longer fails at configure with `No target "hdf5::hdf5-shared"`; distro HDF5 found by CMake's FindHDF5 is now supported.
- README: corrected Linux build, run and test commands (separate Debug tree for the tests), package list and a WSL note.
- Fix Serial and MT running different chemistry with `/chem/env/scavenger O2` under IRT_syn: on MT workers, `O2` produced by two tracked molecules stayed tracked instead of joining the bulk pool. `Species.*` now has no `O_2^0` row in either mode, and MT results with O2 set change statistically. SBS and pure water keep the row.

## 1.2.0 (2026-11-03)

- Add a `Farokhi2023` chemistry (`/chem/select Farokhi2023`) reproducing the FLASH oxygen-depletion radiolysis network of Farokhi et al. 2023 (Radiat. Phys. Chem. 212, 111184).
- Add 8 example macros reproducing the paper's Fig. 2/Fig. 3 (90 MeV proton) and Fig. 5 (0.5 MeV electron) conditions at their O2 concentrations.
- Switch the default physics constructor from `G4EmDNAPhysics` to `G4EmDNAPhysics_option2`, affecting every Chemistry's simulated output.
- Record species G-values at a finer time grid (19 log-spaced points from 1 ps to 1 µs, up from 7).
- Validate and document the `PrimaryKiller` track-segment-truncation feature (`/primaryKiller/eLossMin` etc.), previously undocumented.
- Each dump now writes `TrackLengths.Txt` / `TrackLengths.csv` with one row per event: the primary's track length, its start and end kinetic energy, how it ended (`stopped`, `escaped` or `killed`), and the summed length and count of secondary electrons (first generation and all generations).
- `Manifest.json` has a new `trackLengths` summary: event count, end-reason counts, and the mean and SEM of each column.
- Added `docs/recipes/`: source-verified how-tos for recurring Geant4/Geant4-DNA development tasks in this project (creating a scorer, a species counter, mesoscopic 3D species counting, HDF5 export, a new macro command, dnachem-min's macro command reference, adding a new Chemistry, adding a reaction or molecule, and adding a per-run counter), each including a Serial vs MT section.
- Added a clickable architecture explorer (docs/explorer/index.html): module map, run flow and task guides, with a ctest check that keeps it in sync with the source tree.
- New `/primaryKiller/energyCuts <bool>` command (default true): `false` turns off every primary-electron energy cut and keeps only the box cut.
- `/primaryKiller/minKineticE` now sets the kinetic energy at or below which the primary electron is killed (it previously did nothing).
- Macro builder: "Off" for the primary-electron energy cuts now writes `/primaryKiller/energyCuts false` instead of setting both thresholds to 10 MeV.

## 1.0.0 (2026-10-09)

- New `Tonneau2025` Chemistry (`/chem/select Tonneau2025`): the 73-reaction homogeneous pure-water network of Table 2 of Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, run on tracked molecules from the pre-chemical stage instead of from 100 ns. It includes the acid-base block and the dissolved-O2 reactions.
- A Chemistry can now add molecules of its own; `Tonneau2025` adds HO3, which appears as an extra species column in its species and spatial outputs only.
- New example macro `macro/beam_tonneau2025.in` and a 100 ns G-value comparison script.
- SBS is selectable again: `/process/chem/TimeStepModel SBS|IRT_syn`, with the new `/chem/meso/enable true|false` for the mesoscopic stage (SBS and IRT_syn-only run particle-based only; an explicit `true` with SBS is an error).
- The chemistry end time defaults to 1 us without the mesoscopic stage and 1 s with it; `Manifest.json` records `timeStepModel` and `mesoEnabled`, and no `SpeciesMeso*` files are written when the mesoscopic stage is off.
- `sim` finds the macro as given, then next to the executable, then in `macro/`; output goes to `<exe dir>/results` by default, whose `Manifest.json` indexes every dump.
- New example macros `example_sbs.in`, `example_irt.in` and `example_meso.in` (O2 scavenger, two sub-runs dumped to `sub_01` and `sub_02`).
- SBS keeps reactions fully diffusion-controlled and reproduces the recorded SBS reference (`docs/sbs-regression.md`); `Species.Txt` no longer stops at 5 ns without the mesoscopic stage.
