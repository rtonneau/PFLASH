# Implementation Plan

**Session:** Read ./notebooks/PLAN.md and start ##2.
**Date:** 2026-11-03T17:10:35.316Z
**Estimated effort:** 1 day

## Strategy

Build the notebook in two small, runnable stages: first establish configurable input discovery and visible readers, then add the cross-checked phase analyses. Finish by documenting the executed workflow and its regenerated provenance. This keeps the notebook executable after each commit and avoids inventing reusable G4Utils APIs before the report phase.

## Tickets Overview

- **Ticket 1:** Create the configurable notebook discovery and input-reading foundation.
- **Ticket 2:** Add the physical, pre-chemical, particle-stage, and mesoscopic vertical-slice analyses.
- **Ticket 3:** Document reproduction, provenance, and the verified notebook workflow.

## Sequencing Rationale

The configuration, provenance, and direct readers must exist before plots can use them. The analysis ticket then works from those stable in-notebook data frames and source semantics. Documentation follows successful fresh-kernel execution so it records observed rather than assumed versions and outcomes.

## Risks & Mitigation

- **Risk:** A user points the notebook at a dump with missing optional output. → **Mitigation:** Check required files up front and display clear, stage-specific guidance before skipping optional analysis.
- **Risk:** Particle-stage and mesoscopic measurements are compared as though they had the same normalization or coverage. → **Mitigation:** Mark the 5 ns hand-over, compute mesoscopic means explicitly, and state each stage limitation beside its plot.
- **Risk:** The two-event sample is interpreted as a scientific result. → **Mitigation:** Display event count and workflow-only caveats in the provenance and analysis sections.
- **Risk:** Jupyter MCP remains unavailable to the active tool surface. → **Mitigation:** Confirm the configured server before execution and stop rather than silently replacing the required MCP workflow.

## Assumptions

- The ignored regenerated dumps remain available at their documented paths during implementation and are never committed.
- `E:\\miniconda3\\envs\\GEANT4` keeps its editable G4Utils installation from `D:\\DEV\\PYTHON\\G4Utils`.
- Spatial mapping and export stay outside this first pass; only spatial-file availability and metadata are shown.
