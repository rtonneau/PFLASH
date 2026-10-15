# Session: sparse meso spatial output and format doc

**Date:** 2026-10-03T17:07:57.520Z
**Status:** Grill phase complete

## Problem Statement

`SpeciesMesoSpatial.h5` (PR #20) writes every cell the mesoscopic mesh holds, including empty ones. At 500 ns that is 24,289 cells per snapshot, of which about 900 hold molecules, which gives about 28 MB per 10 keV event at a 1 ms end time. The user accepts writing only cells that contain species. There is also no standalone description of the HDF5 format to build a Python extraction script from.

## Context & Constraints

- **Current behavior:** `MeshTotalsAction::TakeSnapshot` (`src/chemistry/TimeStepAction.cc`) copies every cell from `const_begin()` to `const_end()` into `MesoSpatialFile::Snapshot`. `MesoSpatialFile` (`src/scoring/MesoSpatialFile.cc`) writes `/run<R>/event<E>/snapshot<k>/{position_nm (N×3 f64), counts (N×S u32)}`, with attributes `time_ns` and `cellSize_nm`, root attributes `species`, `formatVersion` = 1 and `units`, and hard links for repeated snapshots. Counts only; there are no concentrations.
- **Pain point:** file size is dominated by empty cells, and the format is only described in a short paragraph of the sim-output skill.
- **Dependencies:** this builds on PR #20 (branch `feat/meso-spatial-snapshots`, not merged into `Meso`). The session branch is stacked on it, and its PR targets `feat/meso-spatial-snapshots`.
- **Tech stack:** C++20, Geant4 11.4.1, vcpkg HDF5 1.14.6 (C++, thread-safe, no deflate filter), h5py in the conda env `GEANT4_py311`.

## Success Metrics

- Each snapshot holds only cells whose summed count over the species columns is > 0. A snapshot with no molecules still exists, with N = 0.
- Root attribute `formatVersion` = 2, and new root attribute `formatDoc` = "docs/output/SpeciesMesoSpatial-h5.md".
- `docs/output/SpeciesMesoSpatial-h5.md` describes the file completely: the layout tree; every attribute and dataset with dtype, shape and unit; the species-column rules; hard links; concentration = count / (N_A · (cellSize_nm·1e-8 dm)³); and a short h5py snippet that loads one snapshot into numpy. A Python script can be written from it alone.
- For one event, the counts summed over cells still equal `SpeciesMeso.csv` at every time (exact). The new file size per event is reported.
- The unit tests pass. The sim-output skill and `CLAUDE.md` no longer say "empty ones included" and point to the new doc.

## Architecture & Approach

- Filter at capture: `TakeSnapshot` skips a cell whose species-column counts are all zero. Cells holding only species outside the columns count as empty. The writer stays generic. It only gains the version bump to 2 and the `formatDoc` attribute, whose value is passed as a constant in `MesoSpatialFile`.
- Layout unchanged otherwise: N×S `counts` and N×3 `position_nm`, where row i of each describes the same cell.
- New markdown spec under `docs/output/`, referenced from the sim-output skill and `CLAUDE.md`.
- Validation: reuse the previous session's `check_meso_spatial.py` logic, copied into this session's `analysis/` and adapted to assert that no row is all zero.

## Assumptions & Trade-offs

- Empty cells carry no information the user needs. The mesh extent is not recorded.
- Sparse triplet storage was considered and rejected: the N×S table is simpler to read and small enough once empty cells are dropped.
- No reader module is shipped, only the spec with an example snippet.
- Old version-1 files are not converted.

## Open Questions

- None.

## Notes

- Glossary: the **Spatial snapshot** term in `CONTEXT.md` now reads "every cell holding at least one molecule".
- Branch: stacked branch from `feat/meso-spatial-snapshots`, PR into that branch (user choice, because gps cannot reuse an existing branch).
