# Ticket 05: manifest-docs

**Acceptance Criteria:**
- [ ] `.claude/.claude-project.json` lists `Manifest.json` in `run.outputs` instead of `EnergyDeposit.Txt`.
- [ ] `.claude/skills/sim-output/SKILL.md` describes `Manifest.json` (per dump, `runs[]`, fields, `files`, `schemaVersion`) instead of `EnergyDeposit.Txt`.
- [ ] `CLAUDE.md` Key Files lists `ManifestWriter` and `RunManifest` and mentions the per-run records in `RunAccumulator`.
- [ ] No file outside historical plans still mentions `EnergyDeposit.Txt`.

**Files to Touch:**
- `.claude/.claude-project.json`
- `.claude/skills/sim-output/SKILL.md`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
grep -rn "EnergyDeposit.Txt" . --exclude-dir=build --exclude-dir=build-ninja --exclude-dir=.work --exclude-dir=.scratch --exclude-dir=plans --exclude-dir=.git
```

Expected:
No output.

**Notes:**

Plan Task 5. `.claude/plans/*` keep their historical mentions on purpose.
