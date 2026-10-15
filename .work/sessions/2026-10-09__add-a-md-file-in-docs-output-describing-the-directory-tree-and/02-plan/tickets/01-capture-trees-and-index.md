# Ticket 01: capture-trees-and-index

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `sim` was run with 10 keV e-, `/run/beamOn 2`, for the default, `spatialOutput true` and end-time <= hand-over cases, and the trees were saved under `.scratch/`.
- [ ] `docs/output/README.md` lists every output file with content, writer, when it appears and typical size.
- [ ] README links to the `sim-output` skill and `SpeciesMesoSpatial-h5.md`.

**Files to Touch:**
- `docs/output/README.md`

**Verification Step:**

Run:
```bash
ls docs/output/README.md && grep -c "Manifest.json" docs/output/README.md
```

Expected:
The file exists and the count is at least 1.

**Notes:**

Follow `.claude/geant4-instructions.md` for the build and run procedure. Take file names from the real output, not from memory.
