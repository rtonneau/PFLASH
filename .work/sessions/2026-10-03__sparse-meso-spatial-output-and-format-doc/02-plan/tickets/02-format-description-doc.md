# Ticket 02: format-description-doc

**Model:** sonnet

**Acceptance Criteria:**
- [ ] New `docs/output/SpeciesMesoSpatial-h5.md` covers:
  - purpose and when the file exists (`/chem/meso/spatialOutput true`, one file per dump, prefix/subdir, `EndOfRun_`);
  - the layout tree;
  - a table of every root attribute (`species`, `formatVersion`, `formatDoc`, `units`), every group attribute (`time_ns`, `cellSize_nm`) and every dataset (`position_nm` N×3 float64 nm, cell centres in the world frame; `counts` N×S uint32 molecules), with dtype, shape and unit;
  - row i of `counts` and of `position_nm` describe the same cell;
  - only cells with ≥ 1 molecule are written, and N can be 0;
  - species-column rules (every molecule-table configuration except water in any state, `G4FakeMolecule` "None" and `(B)` bulk species; Geant4 display names, UTF-8, sorted);
  - snapshot times follow the mesoscopic log grid, k ascending in time; repeated mesh states are hard-linked datasets;
  - the cell side grows as the mesh coarsens;
  - concentration (mol/L) = count / (N_A · (cellSize_nm·1e-8)³);
  - compression only when the HDF5 build has deflate;
  - version history (1 = all cells, 2 = non-empty cells only).
- [ ] The doc includes a short h5py example (≤ 30 lines) that lists events and loads one snapshot into numpy arrays, plus concentration. The example runs successfully on a real v2 file with the conda interpreter.
- [ ] `.claude/skills/sim-output/SKILL.md` and `CLAUDE.md` drop "empty ones included" and link the doc.

**Files to Touch:**
- `docs/output/SpeciesMesoSpatial-h5.md`
- `.claude/skills/sim-output/SKILL.md`
- `CLAUDE.md`

**Verification Step:**

Run (after `./sim beam_meso_spatial.in --dir ../.scratch/tests/2026-10-03__sparse-meso-spatial-output-and-format-doc/t02` from `build/`; extract the example into a scratch `.py` that takes the file path):
```bash
C:/Users/rtonneau/miniconda3/envs/GEANT4_py311/python.exe .scratch/tests/2026-10-03__sparse-meso-spatial-output-and-format-doc/02-example.py .scratch/tests/2026-10-03__sparse-meso-spatial-output-and-format-doc/t02/SpeciesMesoSpatial.h5
```

Expected:
It prints the events and one snapshot's shapes and concentrations, with no exception.

**Notes:**

Write for a reader who has never seen the code. Keep the existing short paragraph in the sim-output skill, but reduce it to a summary with a link.
