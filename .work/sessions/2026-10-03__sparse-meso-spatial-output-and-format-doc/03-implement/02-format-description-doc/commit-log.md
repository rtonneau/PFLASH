# Ticket 02: format-description-doc

**Status:** ✅ Done

## Local Test Result

Ran `02-example.py` (the doc's example) on `t02/SpeciesMesoSpatial.h5` (`beam_meso_spatial.in`, 2 events) with the conda interpreter, rerun in this session. Output: `formatVersion 2 S = 16`; events `run0/event0` and `run0/event1`; `snapshot54: t = 1e+06 ns, cell = 976.6 nm, N = 385`; shapes `(385, 3) (385, 16)`; most abundant `H^0`, max 3.57e-09 mol/L. The file is 6.8 MB for 2 events (v1: about 58 MB).

## Review Notes

Implemented by a sonnet subagent; reviewed in this session with no changes. The doc covers every listed item: purpose and dump rules, layout tree, attribute/dataset table, row correspondence, sparse cells with N = 0, species-column rules, the time grid with hard links, cell coarsening, the concentration formula, compression, a 25-line h5py example, and version history. I checked its claims against `MesoSpatialFile.cc` and `TimeStepAction.cc`. The sim-output skill and `CLAUDE.md` now summarise and link it.

## Blockers / Challenges

None. The gzip statement cannot be exercised here (no deflate filter in the vcpkg HDF5); it is taken from the writer code.

## Commits

- 9a8724f docs(output): describe the SpeciesMesoSpatial.h5 format (ticket 02)

## Time Spent

1m (ticket-start.js to ticket-complete.js)
