# Ticket 02: layout-files

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] One file per layout exists in `docs/output/layouts/`: cwd, prefix, subdir (`--dir` and `/run/dumpDataAndResetToDir`), `EndOfRun_` flush, staging folders.
- [ ] Each file shows trimmed real trees for the three meso cases.
- [ ] Every file name shown appears in the captured run output.

**Files to Touch:**
- `docs/output/layouts/*.md`

**Verification Step:**

Run:
```bash
ls docs/output/layouts
```

Expected:
Five layout files.

**Notes:**

Trim repetition (for example one `PreChemical_run0_event0.txt`, then `...`). Keep descriptions to the layout; file meaning stays in the README.
