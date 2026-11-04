# Notebook series implementation plan

Status: draft for approval. GPS session: 2026-11-04__read-notebooks-plan.md-and-implement-step-5-of-it.

**Goal:** Complete step 5 of notebooks/PLAN.md using the validated G4Utils interfaces.
**Architecture:** Five independently runnable notebooks consume the existing demonstration dumps. Reusable readers and calculations stay in G4Utils; notebook cells handle configuration, explanation and plotting.
**Tech stack:** Jupyter MCP, GEANT4 Python kernel, G4Utils, pandas, NumPy, matplotlib, h5py.
**Spec:** .work/sessions/2026-11-04__read-notebooks-plan.md-and-implement-step-5-of-it/01-grill/resume.md
**Branch:** feat/notebook-series
**Estimated effort:** 1–2 working days after Jupyter MCP becomes available.

## Strategy

Establish discovery and provenance first, then add the stage-specific notebooks, then publish navigation and execution evidence. Create and edit notebooks only through Jupyter MCP. Every notebook has its own configuration/import cells and runs in a fresh GEANT4 kernel without relying on another notebook's state. Retain vertical_slice.ipynb unchanged as historical evidence. Save the approved GPS plan before implementation; GPS creates the session branch in the existing checkout, without a worktree.

## Global Constraints

- Jupyter MCP is required for notebook creation, editing and execution; local execution is not an authorized fallback.
- Inspect the current G4Utils implementation before choosing public interfaces; no replacement exploratory parsers or duplicated reusable calculations.
- Preserve existing edits to notebooks/PLAN.md and notebooks/G4UTILS_REPORT.md and unrelated repository changes.
- Reuse the documented two-event demo dumps; keep generated datasets, exports and test artifacts outside Git.
- Scientific claims require source checks. Inspect configured Geant4 source and the matching 11.4.3 source before assigning pre-chemical meanings; report the config/version mismatch and missing config-resolution document without silently changing configuration.
- Keep sample sizes, units, normalization, stage coverage and limitations next to the interpreted results.
- Stop notebook work if MCP cannot connect; record the prerequisite instead of claiming execution.

## Tickets Overview

1. Discovery, configuration and provenance notebook.
2. Physical and physico-chemical notebooks.
3. Chemical species, yields and reactions notebook.
4. Optional mesoscopic spatial notebook and selected-snapshot export.
5. README, full-series verification and completion evidence.

## Sequencing Rationale

Ticket 1 establishes the input-selection pattern. Tickets 2–4 repeat that pattern while remaining independently runnable. Ticket 5 verifies the complete user workflow and documents only results observed during execution. Commit each ticket after its checks pass.

## Risks & Mitigation

- **Risk:** MCP remains unavailable. → **Mitigation:** require a connected MCP session and fresh GEANT4 kernel before creating notebook files.
- **Risk:** an editable G4Utils checkout has changed since the report. → **Mitigation:** record the actual revision and kernel import paths, then verify the required interfaces before use.
- **Risk:** source-version differences produce incorrect labels. → **Mitigation:** verify against source; retain raw codes and an explicit limitation when the meaning cannot be established.
- **Risk:** a full-world spatial export consumes excessive memory. → **Mitigation:** select one event/time/species and use a bounded export supported by G4Utils, inspecting size before materializing dense data.
- **Risk:** pre-existing changes enter a ticket commit. → **Mitigation:** review diffs and stage only the ticket's changes.

## Assumptions

- The two existing demo dumps are the default inputs after checking their files on disk.
- No simulation or G4Utils implementation changes are planned. A library defect that blocks delivery must be reported before expanding scope.
- The historical notebook remains available, but the README points new readers to the numbered series.

## Review Focus

- Missing required inputs: ticket 1 checks regeneration guidance without obscure parser failures.
- Results indexes and prefixed dumps: ticket 1 checks explicit discovery/selection without combining unrelated dumps.
- Empty or missing optional records: tickets 2 and 4 check explanatory skips without misleading empty plots.
- Different stage/time/energy normalization: ticket 3 checks separate observables and labels against source rows.
- Hidden kernel state and large exports: ticket 4 bounds export; ticket 5 runs every notebook in its own fresh kernel.

--- ticket: 01-discovery-provenance ---

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.41)
**Effort:** medium

**Acceptance Criteria:**
- [ ] Connect through Jupyter MCP, start a fresh GEANT4 kernel, and verify G4Utils import paths, revision and required APIs. Stop if MCP is unavailable.
- [ ] Create 00_discovery_provenance.ipynb through MCP with configurable input paths, directory/index discovery and explicit dump selection using G4Utils.
- [ ] Display provenance, chemistry mode, seeds, event count, deposited energy, settings and actual available files. Include sample-size and source-revision limitations.
- [ ] Check the default dumps plus a scratch prefixed/indexed layout; demonstrate that selection identifies one dump. Missing required paths explain how to reproduce the data.

**Files to Touch:**
- `notebooks/00_discovery_provenance.ipynb`

**Verification Step:**

Run all notebook cells through Jupyter MCP in a fresh kernel. In a temporary verification cell in that kernel, run:
```python
import sys
from pathlib import Path
import g4utils
assert Path(sys.executable).is_file()
assert Path(g4utils.__file__).is_file()
print("PASS: kernel and G4Utils provenance")
```
Expected: the PASS line; all notebook cells complete; each default dump reports 2 events and 20,000 eV; scratch discovery selects the requested dump; a missing path produces regeneration guidance. Inspect actual files rather than trusting manifest entries alone.

**Notes:**

Read notebooks/README.md, G4UTILS_REPORT.md and the current G4Utils source first. Use supported discovery APIs; do not teach the old unprefixed-only restriction. Keep temporary checks/artifacts in the session scratch directory. Review outputs, then commit this notebook.

--- ticket: 02-physical-prechemical ---

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.29)
**Effort:** high

**Acceptance Criteria:**
- [ ] Create independently runnable 01_physical_interactions_tracks.ipynb and 02_physicochemical_records_positions.ipynb through MCP, each repeating the configuration/provenance pattern from ticket 1.
- [ ] Use G4Utils readers for discrete interaction counts, event-level primary/secondary track lengths, water modifications and solvated-electron positions. Preserve run/event identity and displayed units.
- [ ] Check representative reader values against original files. Verify field meanings against simulation and Geant4 .cc source before labeling modification codes; keep raw codes if unverified.
- [ ] Explain aggregate process counts versus histories, non-aborted track-length rows and pre-chemical coverage beside plots. Empty/missing optional records produce guidance.

**Files to Touch:**
- `notebooks/01_physical_interactions_tracks.ipynb`
- `notebooks/02_physicochemical_records_positions.ipynb`

**Verification Step:**

Run both notebooks through MCP, each in a new kernel; inspect tables and rendered plots. Run this in each kernel after Run All:
```python
import matplotlib.pyplot as plt
assert not any(not ax.get_xlabel() or not ax.get_ylabel()
               for n in plt.get_fignums()
               for ax in plt.figure(n).axes if ax.has_data())
print("PASS: plotted axes labeled")
```
Expected: all cells complete and axes have labels; manual plot review confirms units, legends and sample sizes. Representative rows match raw records. Scratch empty/missing optional data produces an explanation rather than a misleading plot. The axis check supplements visual review and may find no live figures after inline rendering.

**Notes:**

Inspect project output documentation and G4Utils reader contracts. Do not infer complete tracks or physical-step histories. No physics implementation changes. Review both notebooks, then commit together as the physical-to-pre-chemical analysis pair.

--- ticket: 03-chemical-observables ---

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.35)
**Effort:** high

**Acceptance Criteria:**
- [ ] Create independently runnable 03_chemical_species_yields_reactions.ipynb through MCP using G4Utils species, yield, reaction and aggregate mesoscopic interfaces.
- [ ] Explain and plot particle-stage species/yields and reaction counts with explicit time, count and energy units. Name the yield estimator; do not equate ratio-of-totals and mean-of-event yields or invent uncertainty unsupported by stored data.
- [ ] Plot optional mesoscopic mean counts per event separately from particle G-values. Identify hand-over and separate seeded inputs; do not splice them into one trajectory.
- [ ] Explain omitted bulk/scavenger and mesoscopic reaction coverage; validate representative calculations and missing aggregate behavior against the report and source files.

**Files to Touch:**
- `notebooks/03_chemical_species_yields_reactions.ipynb`

**Verification Step:**

Run all cells through MCP in a fresh kernel and compare representative library results with source rows and recorded report checks. Run:
```python
from g4utils.DnaChem import Dump, load_species, load_reactions
print("PASS: chemical analysis interfaces imported")
```
Expected: the PASS line and successful notebook execution. Representative counts and normalizations match raw data, including the report's mesoscopic 97/2 example when selecting its recorded row. Reviewed plots distinguish molecules/event from molecules/100 eV and show hand-over. With aggregate input absent, its section gives guidance while particle analysis still completes.

**Notes:**

Read the report's estimator decisions and current APIs before implementation. Optional absence must not conceal malformed required data. Commit after numerical and visual checks.

--- ticket: 04-spatial-snapshot-export ---

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.42)
**Effort:** high

**Acceptance Criteria:**
- [ ] Create independently runnable 04_mesoscopic_spatial_snapshots.ipynb through MCP with configurable event, snapshot time and species selection.
- [ ] Use G4Utils HDF5 interfaces to display available snapshots, actual cell sizes and one selected spatial view with units and count/concentration meaning stated.
- [ ] Demonstrate a bounded export of the selected snapshot into the ignored scratch area using an existing G4Utils export interface. Check bounds and estimated allocation before dense export.
- [ ] Missing optional HDF5 explains the spatial-output macro setting and reproduction steps; unavailable selections list valid choices. No dense full-world export by default.

**Files to Touch:**
- `notebooks/04_mesoscopic_spatial_snapshots.ipynb`

**Verification Step:**

Run all cells through MCP in a fresh kernel, using the mesoscopic demo. Have the export cell assign its returned output path to export_path; in the same kernel run:
```python
from pathlib import Path
assert Path(export_path).is_file()
assert Path(export_path).stat().st_size > 0
print("PASS: selected snapshot exported")
```
Expected: the PASS line; export metadata matches the chosen event/time/species, bounds and units; selected counts match the source snapshot. Inspect the spatial plot. Repeat with missing optional HDF5 and an invalid selection to confirm guidance. Exported files remain untracked.

**Notes:**

Read docs/output/SpeciesMesoSpatial-h5.md and the actual G4Utils export API. Use the snapshot's cellSize_nm, including mesh coarsening. Verify exported structure and values rather than file existence alone. Commit the notebook after review.

--- ticket: 05-document-verify-series ---

**Model:** heavy
**Model (Jev):** opus-5.5 (confidence 0.25)
**Effort:** high

**Acceptance Criteria:**
- [ ] Update README navigation for 00–04, retain a historical link to vertical_slice.ipynb, and document setup, configurable paths, current G4Utils requirements, reproduction commands and expected outputs.
- [ ] Run every numbered notebook in its own fresh kernel through MCP following the README; review saved tables and plots, units, normalization, limitations and optional-data guidance.
- [ ] Add an execution record with dates, actual kernel/library/source revisions, data provenance, numerical checks, plot review, optional-path checks and unresolved limits. Do not claim verified physics from two-event demos.
- [ ] Update step 5 checkboxes only after all acceptance criteria pass. Preserve earlier edits and keep generated data/exports out of commits.

**Files to Touch:**
- `notebooks/README.md`
- `notebooks/VALIDATION.md`
- `notebooks/PLAN.md`
- `notebooks/00_discovery_provenance.ipynb`
- `notebooks/01_physical_interactions_tracks.ipynb`
- `notebooks/02_physicochemical_records_positions.ipynb`
- `notebooks/03_chemical_species_yields_reactions.ipynb`
- `notebooks/04_mesoscopic_spatial_snapshots.ipynb`

**Verification Step:**

After the MCP fresh-kernel runs and saved-output inspection, run from the repository root:
```powershell
git diff --check
git status --short
```
Expected: no whitespace errors; only intended notebook/documentation changes are staged, with original unrelated changes preserved. All five notebooks have a recorded successful fresh-kernel run, reviewed outputs and optional-data checks. README links resolve, reproduction commands reference existing macros, and the historical notebook has no diff.

**Notes:**

This is the final acceptance pass; avoid repeating successful runs without changes or unresolved concerns. Record blockers as blockers, not completed checks. Commit the documentation and final verified notebook outputs after reviewing the complete diff. GPS finish is a separate workflow command.
