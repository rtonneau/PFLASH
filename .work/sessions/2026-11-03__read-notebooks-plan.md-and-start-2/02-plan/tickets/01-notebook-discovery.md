# Ticket 01: notebook-discovery

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.49)
**Effort:** medium

**Acceptance Criteria:**
- [ ] Add `notebooks/vertical_slice.ipynb` with editable IRT and mesoscopic dump paths and actionable failures for unavailable required or optional files.
- [ ] Show dump provenance, manifest settings, event and energy totals, and discovered files using current G4Utils interfaces.
- [ ] Include compact, visible readers for physical interactions, track lengths, pre-chemical records, and aggregate mesoscopic input formats without adding G4Utils APIs.

**Files to Touch:**
- `notebooks/vertical_slice.ipynb`

**Verification Step:**

Run:
```bash
Jupyter MCP: execute `notebooks/vertical_slice.ipynb` in a fresh GEANT4 kernel through the discovery section.
```

Expected:
The configuration and provenance cells complete without errors and identify the two documented PureWater dumps.

**Notes:**

Use the regenerated ignored dump paths only as defaults; never commit output data. `Dump` exposes manifests, particle-stage species/reactions, and spatial HDF5; keep the other small readers visible in the notebook.
