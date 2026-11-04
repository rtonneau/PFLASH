# Ticket 03: notebook-reproduction-docs

**Model:** fast
**Model (Jev):** haiku-5.5 (confidence 0.64)
**Effort:** low

**Acceptance Criteria:**
- [ ] Update `notebooks/README.md` with the vertical-slice notebook, editable input paths, current GEANT4 environment, and exact regenerated provenance.
- [ ] Correct the documented Geant4 tag from patch-01 to patch-03 and retain the documented mesh-cap warning and limitations.
- [ ] Record the fresh-kernel execution outcome without adding generated dump files to Git.

**Files to Touch:**
- `notebooks/README.md`
- `notebooks/vertical_slice.ipynb`

**Verification Step:**

Run:
```bash
git diff --check
```

Expected:
No whitespace errors; README instructions match the executed notebook and regenerated manifests.

**Notes:**

The editable G4Utils install is `D:\\DEV\\PYTHON\\G4Utils` in `E:\\miniconda3\\envs\\GEANT4`; run the final notebook from a fresh kernel before recording its outcome.
