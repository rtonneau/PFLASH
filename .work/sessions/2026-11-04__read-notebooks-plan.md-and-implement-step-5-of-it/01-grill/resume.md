# Session: Read './notebooks/PLAN.md' and implement step 5 of it

**Date:** 2026-11-04T06:42:35.151Z
**Status:** Grill phase complete

## Problem Statement

Implement step 5 of notebooks/PLAN.md: complete an explanatory, reproducible notebook series using the validated G4Utils interfaces, with setup and navigation documentation and fresh-kernel execution evidence.

## Context & Constraints

- **Current behavior:** Steps 1–4 are recorded as completed. The repository has vertical_slice.ipynb, two demonstration macros, a README and the G4Utils report. The numbered series does not yet exist, and the README still describes exploratory readers. The required G4Utils additions are now merged into its main branch.
- **Pain point:** The exploratory first pass must become an independently runnable series that demonstrates the supported analysis interfaces and clearly explains scientific and data limitations.
- **Dependencies:** Retain the explicit requirement to use Jupyter MCP for notebook creation, editing and execution. The user declined the local Jupyter tools fallback and will enable MCP. Reuse the documented demonstration data and validated G4Utils interfaces. Preserve existing unrelated edits. Use a GPS session branch in the current primary checkout; do not create a worktree.
- **Tech stack:** Jupyter notebooks with the GEANT4 Python kernel, G4Utils, pandas, NumPy, matplotlib and h5py; dnachem-min demonstration dumps and their reproduction macros.

## Success Metrics

- Notebooks 00–04 cover discovery/provenance, physical interactions/track lengths, physico-chemical records/positions, chemical species/yields/reactions, and optional mesoscopic spatial snapshots/export, respectively. Each notebook is independently runnable.
- The notebooks README documents configurable inputs, setup, reproduction commands, navigation and expected outputs. Optional analyses and actionable missing-data guidance are explicit.
- Each notebook is executed in a fresh kernel through Jupyter MCP on the documented data. Tables, plots, labels and units are inspected, optional missing-data behavior is checked, and execution outcomes and limitations are recorded.
- Reusable parsing and calculations in the new series use validated G4Utils interfaces. Scientific limitations are visible at the point of interpretation, as required by PLAN.md.

## Architecture & Approach

Retain vertical_slice.ipynb as the historical first-pass example. Build the five independently runnable numbered notebooks around the existing demo inputs and G4Utils capabilities. Organize the material as specified in step 5, with provenance and configurable paths available to each notebook. Limit the optional spatial export demonstration to a selected snapshot. Update the README to guide readers through the series and explain the environment, data reproduction, expected results and optional analyses. Execute and review the series through Jupyter MCP before recording completion.

## Assumptions & Trade-offs

The small seeded datasets demonstrate workflows rather than converged scientific results. Preserve distinctions between particle-stage G-values and mesoscopic aggregate counts, including normalization and hand-over boundaries; do not present separate seeded simulations as a continuous trajectory. Preserve reaction-coverage and pre-chemical interpretation limitations from PLAN.md and the report. Retaining the historical notebook preserves the first-pass evidence while the new series becomes the documented entry point. A selected-snapshot export bounds the optional spatial demonstration.

## Open Questions

No remaining scope decisions from the grill. Jupyter MCP tools were not exposed in this chat at the latest check; their availability is an implementation prerequisite, not permission to substitute local Jupyter execution. Source-version verification remains an implementation check: project configuration selects Geant4 11.4.1 source while the active install and demonstration data report 11.4.3, and the referenced global CONFIG_RESOLUTION.md was not found. Do not guess physics/API behavior or silently remove those limitations.

## Notes

The user approved the scope and explicitly requested saving this design with $gps write while retaining Jupyter MCP. They authorized a GPS session branch in the existing checkout, overriding the earlier suggestion to stay on main. Existing edits to notebooks/PLAN.md and notebooks/G4UTILS_REPORT.md predate this implementation and must be preserved. This records the approved design only; branch creation belongs to the GPS plan workflow.
