# Ticket 02: vertical-slice-analysis

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.42)
**Effort:** high

**Acceptance Criteria:**
- [ ] Add physical interaction and event-level track-length plots, pre-chemical water-modification and solvated-electron position views, particle-stage species/reaction analysis, and mesoscopic aggregate-count plots.
- [ ] Label units, hand-over time, normalization, two-event sample size, and particle-versus-mesoscopic limitations beside every interpretation.
- [ ] Cross-check representative parsed rows and the G-value/mean-count calculations against raw files and current scoring/source semantics.

**Files to Touch:**
- `notebooks/vertical_slice.ipynb`

**Verification Step:**

Run:
```bash
Jupyter MCP: execute `notebooks/vertical_slice.ipynb` top to bottom in a fresh GEANT4 kernel.
```

Expected:
All three phases render checked tables or plots without an exception; optional spatial data is reported as metadata and no spatial map is presented.

**Notes:**

Use `PhysicsInteractions.csv` as aggregate discrete process counts, `TrackLengths.csv` as non-aborted event rows, and `SpeciesMeso.csv` as summed counts that must be divided by `totalEvents` for a per-event curve. Keep spatial visualization deferred to the planned `04` notebook; do not present the small run as converged physics.
