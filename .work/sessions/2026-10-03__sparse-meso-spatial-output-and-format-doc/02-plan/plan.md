# Implementation Plan

**Session:** sparse meso spatial output and format doc
**Date:** 2026-10-03T17:12:10.173Z
**Estimated effort:** 2–3 hours

## Strategy

Change the data first: sparse capture plus format version 2 and `formatDoc`. Then write the format description against the real v2 file. Last, validate end to end against `SpeciesMeso` and measure the new size.

## Tickets Overview

- **Ticket 1:** Write only cells with at least one molecule; bump `formatVersion` to 2; add the root attribute `formatDoc`.
- **Ticket 2:** Write `docs/output/SpeciesMesoSpatial-h5.md` (full format description plus an h5py example) and point the sim-output skill and `CLAUDE.md` to it.
- **Ticket 3:** Smoke validation: summed counts equal `SpeciesMeso` for one event, no all-zero rows, v2 attributes present; report the size per event.

## Sequencing Rationale

The doc describes the v2 file, so it comes after ticket 1. The validation runs on the final code and checks the doc's claims.

## Risks & Mitigation

- **Risk:** a cell holding only species outside the columns is dropped and its molecules vanish from the file → **Mitigation:** none of those species reach the mesh (water, `None` and `(B)` are excluded already). Ticket 3's exact sum check against `SpeciesMeso` would catch a loss.
- **Risk:** the doc drifts from the code → **Mitigation:** ticket 2 writes it from a real v2 file, and ticket 3's script follows the doc's snippet.

## Assumptions

- PR #20 code (branch `feat/meso-spatial-snapshots`) is the base; this branch is stacked on it.
- h5py is available only in the conda env `GEANT4_py311` (`C:/Users/rtonneau/miniconda3/envs/GEANT4_py311/python.exe`).
