# Ticket 01: spec-core

**Model:** opus

**Acceptance Criteria:**
- [ ] `~/dotfiles/claude/skills/g4run/SPEC.md` exists with sections: Purpose & scope, Configuration, Storage layout, Database schema, Run lifecycle, Enqueue, Selectors.
- [ ] Configuration defines the `g4run` block of `.claude-project.json` (memory default, analysis registrations) and global settings (runs root, thread budget, free-RAM margin, python), reusing existing `build`/`run` fields; only `${DEV_DIR}`/`${G4_ROOT}` paths.
- [ ] Database schema lists tables `projects`, `batches`, `runs`, `run_tags`, `analyses` with columns and types, WAL mode, and the state set queued/running/succeeded/failed/cancelled/lost/pruned with allowed transitions.
- [ ] Enqueue documents `{{key}}` rendering, `--set`, `--sweep` cartesian product, `--threads`, `--mem`, `--at`, `--tag`, batch name/note, copying the whole project `macro/` dir with only the main macro rendered, exe + DLL snapshot into the run dir, provenance fields, run-id format `YYYYMMDD-HHMMSS-<macro>-<4hex>`.
- [ ] Commit in dotfiles on branch `docs/g4run-spec` with only SPEC.md staged.

**Files to Touch:**
- `~/dotfiles/claude/skills/g4run/SPEC.md`

**Verification Step:**

Run:
```bash
grep -E '^## ' ~/dotfiles/claude/skills/g4run/SPEC.md; grep -c TBD ~/dotfiles/claude/skills/g4run/SPEC.md
```

Expected:
The seven section headings listed above; TBD count 0.

**Notes:**

Source: `01-grill/resume.md`. Undefined placeholder in a rendered macro is an enqueue error. Unknown `--set` keys too. A sweep over k values × m values yields k·m runs in one batch.
