# Ticket 03: capture-spatial-snapshots

**Status:** ✅ Done

## Local Test Result

`./sim spatial_smoke.in --dir ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t03c` (scratch macro: 10 keV e-, `/chem/meso/spatialOutput true`, `/scheduler/endTime 1 ms`, `/run/beamOn 2`). Exit 0, no FatalException, no MesoSpatialWriteFailed. `t03c/.pending_meso_spatial/SpeciesMesoSpatial.h5` is 59.9 MB with `/run0/event0` and `/run0/event1`, 55 snapshots each (5 ns → 1 ms) and 16 species columns. Cells per snapshot range from 566 at hand-over to a peak of 24289 at 500 ns (only about 900 of them non-empty); cell size goes from 15.26 nm to 976.6 nm. All 13 ctest tests pass in build-ninja. With the switch off, no `.pending_meso_spatial` folder is created (the subagent's run).

## Review Notes

Implemented by an opus subagent. The capture logic matches the criteria: one snapshot per Record/FinishRecording call, shared by every record time it passes; every cell with its centre and resolution; event data cleared in StartRecording and after the write; a JustWarning on failure.

Changes after review:
- **Species columns:** the userID filter let in `H2O^-1`, `H2O^0`, `H2O^1` (the water states of the dissociation channels) and `None^0` (`G4FakeMolecule`), which gave 20 columns. Added `IsSpatialSpecies()`, which excludes every `G4H2O` and `G4FakeMolecule` configuration as well as `(B)`, leaving 16 columns.
- **UTF-8 strings** (`MesoSpatialFile.cc`, the ticket 02 file): Geant4 display names contain a `°` (`HO_2°`, `°OH`), so string attributes are now written with `H5T_CSET_UTF8`. The test reads UTF-8 and includes a non-ASCII species.
- **Compression** (`MesoSpatialFile.cc`): this vcpkg HDF5 build has no deflate filter (`H5_HAVE_FILTER_DEFLATE` undefined), so `setDeflate(4)` was a silent no-op and the 4096-row chunks padded every dataset (72 MB vs 60 MB). Chunking and gzip are now used only when `H5Zfilter_avail(H5Z_FILTER_DEFLATE)`; otherwise datasets are contiguous. The test expects the matching filter count.

## Blockers / Challenges

- The vcpkg HDF5 has no zlib, so the output is uncompressed: about 30 MB per 10 keV event at a 1 ms end time. Rebuilding hdf5 with its `zlib` feature would enable gzip with no code change. This is the user's decision (environment change).
- Most cells written are empty (the mesh keeps every touched cell; decided in the grill).
- h5py is only available in the conda env `GEANT4_py311`.

## Commits

- 6f3d14b feat(meso): capture spatial snapshots of the mesoscopic mesh per event (ticket 03)

## Time Spent

10m (ticket-start.js to ticket-complete.js)
