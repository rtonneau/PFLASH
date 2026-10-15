# Implementation Plan

**Session:** export mesoscopic species concentrations
**Date:** 2026-10-03T15:29:47.910Z
**Estimated effort:** 1–2 days

## Strategy

Work bottom-up. First the switch, then a portable HDF5 writer with its own round-trip test, then capture of the mesh in `TimeStepAction`, then hooking it into the dump and the docs, and last a smoke validation against `SpeciesMeso`. Each ticket builds on code that is already committed. The writer is pure (std + HDF5), so most of the format is pinned by the unit test before Geant4 is involved.

## Tickets Overview

- **Ticket 1:** Add the `/chem/meso/spatialOutput` switch (`MesoSettings::Values::spatialOutput`, default false).
- **Ticket 2:** Portable `MesoSpatialFile` HDF5 writer and staging helpers, with a round-trip unit test.
- **Ticket 3:** Capture Spatial snapshots of the mesh at the record times in `TimeStepAction` and append each event to the staged file.
- **Ticket 4:** Move the staged file into the dump (prefix/subdir, Manifest, `EndOfRun_` safety net), plus a macro and the docs.
- **Ticket 5:** Smoke validation, Serial and MT: summed snapshot counts equal `SpeciesMeso` for one event, and the switch off changes nothing.

## Sequencing Rationale

Ticket 3 needs the switch (1) and the writer API (2). Ticket 4 needs the staged file that 3 produces. Ticket 5 checks the whole chain end to end, so it comes last.

## Risks & Mitigation

- **Risk:** file size or run time grows sharply with about 100 snapshots × all mesh cells per event → **Mitigation:** opt-in switch, gzip level 4, hard links for repeated snapshots; ticket 5 measures the size.
- **Risk:** HDF5 C++ API is used from several worker threads → **Mitigation:** one process-wide mutex inside `MesoSpatialFile::AppendEvent` around every HDF5 call; the file is opened and closed per event.
- **Risk:** zero-cell snapshot (empty mesh) breaks chunked dataset creation → **Mitigation:** writer test covers N = 0; use an unlimited max dimension or skip chunking when N = 0.
- **Risk:** the Debug test binary links a differently built HDF5 → **Mitigation:** link the test against the same `hdf5::hdf5-shared hdf5::hdf5_cpp-shared` targets as `sim`.

## Assumptions

- vcpkg HDF5 1.14.6 (thread-safe build, C++ component) stays the linked HDF5.
- Event and run IDs come from the Geant4 kernel (`G4EventManager` current event, the worker's current run); event IDs are unique within a run in MT.
- The species columns match `SpeciesMeso`: every molecule-table configuration except `H2O` and userIDs ending in `(B)`, by display name (`GetName()`), sorted.
