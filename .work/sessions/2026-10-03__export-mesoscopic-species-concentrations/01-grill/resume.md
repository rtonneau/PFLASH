# Session: export mesoscopic species concentrations

**Date:** 2026-10-03T15:12:03.712Z
**Status:** Grill phase complete

## Problem Statement

The mesoscopic stage only exports species totals summed over the whole mesh (`SpeciesMeso.Txt`/`.csv`, mean count per event vs. time). The spatial distribution of species in the mesoscopic cells is lost. We need an opt-in export of per-event **Spatial snapshots**: for each record time, every cell's position, the cell size and the molecule count of each species.

## Context & Constraints

- **Current behavior:** `MeshTotalsAction` (`src/chemistry/TimeStepAction.cc`) records species counts summed over all cells at the times of the mesoscopic log grid (`MesoSettings::LogTimeGrid`, hand-over time to end time, `/chem/meso/timesPerDecade` per decade). These go into `MesoSpeciesCounter` → `SpeciesMeso.*`. No per-cell data is written.
- **Pain point:** there is no way to study where species sit or how they spread during the mesoscopic stage, or to compute local concentrations.
- **Dependencies:**
  - `G4DNAMesh` (Geant4 11.4.1) is sparse: it is a vector of `(Index, Box, map<species, count>)` cells that exist once touched.
  - `G4DNAEventScheduler` coarsens the mesh by halving pixels per side (`ReVoxelizing(fPixel/2)`), so cell size changes over time.
  - The output pipeline has to fit in: staging under the output directory, `/run/dumpDataAndReset [prefix]` / `dumpDataAndResetToDir`, the `EndOfRun_` safety net, `Manifest.json` (`RunManifest`), and `OutputDir::Resolve`.
- **Tech stack:** C++20, Geant4 11.4.1 DNA (IRT_syn + G4DNAEventScheduler), HDF5 1.14.6 from vcpkg (thread-safe build, C and C++ components already linked into `sim`), CTest with plain `assert`.

## Success Metrics

- With `/chem/meso/spatialOutput true`, a dump writes one `<prefix>SpeciesMesoSpatial.h5` holding `/run<R>/event<E>/snapshot<k>/` for every event and every time of the mesoscopic log grid. Each snapshot has `position_nm` (N×3 float64, cell centres in the world frame), `counts` (N×S uint32), and the attributes `time_ns` and `cellSize_nm`.
- The file root has the fixed species list (all tracked species, no bulk `(B)`/scavenger species; this is the column order of `counts`), the units and a format version. The file is listed in `Manifest.json`.
- For a 1-event run, each snapshot's counts summed over cells equal the `SpeciesMeso` values at the same time.
- With the switch off (the default), no HDF5 file is created and output is unchanged.
- The HDF5 writer has a round-trip unit test that passes in `build-ninja` (Debug). The smoke run (10 keV e-, `/run/beamOn 2`, `/scheduler/endTime 1 ms`) succeeds in Serial and in MT (`--threads 2`).

## Architecture & Approach

- **Switch:** `/chem/meso/spatialOutput true|false` (PreInit, default `false`), added to `MesoMessenger` and stored in `MesoSettings::Current()`.
- **Capture:** when the switch is on, `MeshTotalsAction` records at each record time a full copy of the mesh. That copy includes every cell the mesh holds, empty cells too, with its centre (from the cell `Box`), the cell side (`GetResolution()`) and counts per species in the fixed species order. When one scheduler step passes several record times, the repeated snapshots are stored as HDF5 hard links to the first one instead of copies.
- **Writer:** a portable HDF5 writer class (standard library + HDF5 only, no Geant4 kernel) appends one event's snapshots under `/run<R>/event<E>/` to a staged file in the output directory. It uses chunked, gzip level 4 datasets. A process-wide mutex guards it: the HDF5 C library is thread-safe, the C++ API is not.
- **File granularity:** one `.h5` per dump. Events are appended at the end of each event's mesoscopic stage, so nothing piles up in memory. `/run/dumpDataAndReset` closes the staged file and moves it into the dump with the prefix and subdirectory rules (as `PreChemicalFiles::MoveStaged` does), lists it in `Manifest.json`, and resets. The `EndOfRun_` safety net covers it too.
- **Quantity:** integer counts only. Concentration is derived in analysis as `n / (N_A · side³)`.

## Assumptions & Trade-offs

- Per event only: no aggregation across events, because each event's track sits at different positions.
- Every cell the mesh holds is written, empty ones included. This is still sparse relative to the full grid.
- The time grid is the same as `SpeciesMeso` (`/chem/meso/timesPerDecade`, default 10). That gives about 100 snapshots per event, so files can be large; hard links and gzip mitigate this.
- One file per dump, written serially under a mutex. A crash mid-write can corrupt the staged file. We accept this in exchange for having a single file.
- No concentration column and no integer mesh index: positions are cell centres in nm.
- No CSV output.

## Open Questions

- None blocking. The real file size per event at 10/100 keV will be measured during the smoke run.

## Notes

- Glossary: **Spatial snapshot** was added to `CONTEXT.md` (the project glossary). `.work/GLOSSARY.md` points to it.
- No ADR: none of these choices is hard to reverse.
- Implementation choices made in the grill and approved with the summary: gzip level 4, hard links for repeated snapshots, a round-trip unit test of the writer, and the smoke check that summed counts equal `SpeciesMeso`.
