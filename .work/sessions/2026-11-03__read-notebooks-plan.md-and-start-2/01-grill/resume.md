# Session: Read ./notebooks/PLAN.md and start ##2.

**Date:** 2026-11-03T16:24:30.812Z
**Status:** Grill phase complete

## Problem Statement

Create the first executable vertical-slice Jupyter notebook for `dnachem-min`. It must let a reader inspect documented physical, pre-chemical, particle-stage chemical, and mesoscopic outputs from configurable dump paths without presenting the small demonstration data as validated physics.

## Context & Constraints

- **Current behavior:** `notebooks/PLAN.md` section 2 is unimplemented: `notebooks/` has reproduction documentation and two macros, but no analysis notebook. The documented two-event IRT and mesoscopic-spatial dumps have now been regenerated under the ignored `.scratch/tests/2026-11-03__read-notebooks-plan.md-and-implement-1-establish-the/` directory.
- **Pain point:** The outputs span several raw formats and stages. Existing G4Utils reads manifests, particle-stage species/reactions, and mesoscopic spatial HDF5, but it has no readers yet for physics interactions, track lengths, pre-chemical records, or aggregate mesoscopic counts.
- **Dependencies:** Keep dump paths user-editable; do not commit generated data. Verify parsed values against source files and the current scoring behavior. Preserve the distinction between particle-stage and mesoscopic observables, show units and sample size, and handle optional files with actionable guidance. Update provenance documentation for the regenerated Geant4 patch level.
- **Tech stack:** The `E:\\miniconda3\\envs\\GEANT4` Python environment with pandas, NumPy, h5py, matplotlib, ipykernel/Jupyter, and editable G4Utils from `D:\\DEV\\PYTHON\\G4Utils`. Jupyter MCP is configured for Codex and should be used for notebook creation and execution when its tools are exposed to the chat.

## Success Metrics

- `notebooks/vertical_slice.ipynb` runs top-to-bottom in a fresh configured Jupyter kernel against the documented IRT and mesoscopic-spatial dumps, with editable input paths and clear failure guidance for missing optional files.
- It displays provenance and available files; demonstrates physical interaction counts and event-level track lengths; parses pre-chemical water modifications and solvated-electron positions; plots particle-stage species/reactions; and plots mesoscopic aggregate counts with the hand-over identified.
- It uses G4Utils where its current interfaces fit, visibly documents direct exploratory readers and assumptions elsewhere, labels all units/normalization/limitations, and cross-checks representative raw rows and calculations against the source files and scoring semantics.

## Architecture & Approach

Add one compact `notebooks/vertical_slice.ipynb` with a top configuration cell for the two dump directories. Start with a provenance/files section based on `Dump`/`Manifest`, then use G4Utils for particle-stage species and reaction tables and mesoscopic-spatial metadata. Keep short, visible pandas readers in the notebook for `PhysicsInteractions.csv`, `TrackLengths.csv`, the fixed-width pre-chemical event records, and `SpeciesMeso.csv`. Plot aggregate physical counts, per-event lengths, water-modification and solvated-electron positions, particle-stage species/G-values and reactions, and mesoscopic mean counts per event. Mark the 5 ns hand-over and avoid combining the mesoscopic counts with particle-stage G-values. Add concise source-row and formula checks, a missing-optional-data message path, and update `notebooks/README.md` to record the regenerated patch-03 provenance and current environment.

## Assumptions & Trade-offs

The two-event, 10 keV PureWater runs are reproducible workflow demonstrations, not converged scientific evidence. The first notebook uses the regenerated IRT-only and IRT_syn-plus-mesoscopic dumps; it defers spatial maps and export to the future `04` notebook, while still reporting HDF5 availability and metadata. It does not add G4Utils APIs in this phase; observed reusable gaps are reserved for the G4Utils report and later library work. The known `WrongResolution` warning is explained as the documented 65536-pixel cap, and snapshot cell size is read from data rather than assumed.

## Open Questions

Jupyter MCP is enabled in the Codex configuration but its tools are not currently injected into this active chat. Notebook implementation and execution must use that MCP once the tool surface reconnects; no CLI substitute is assumed without a later explicit change to this requirement.

## Notes

The regenerated IRT dump has 12 required nonempty files; the mesoscopic-spatial dump has 15, including a 6.8 MB format-version-2 HDF5 file with two events and 55 snapshots per event. Both manifests report 20,000 eV total deposited energy and `geant4-11-04-patch-03`; the existing notebooks README says patch-01 and needs correction. G4Utils 0.4.0 is installed editable in the GEANT4 environment and its intended imports succeed.
