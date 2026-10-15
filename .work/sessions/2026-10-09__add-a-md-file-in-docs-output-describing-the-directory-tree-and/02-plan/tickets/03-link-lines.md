# Ticket 03: link-lines

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] One link line to `docs/output/README.md` in `.claude/skills/sim-output/SKILL.md`, `CLAUDE.md` and `docs/output/SpeciesMesoSpatial-h5.md`.

**Files to Touch:**
- `.claude/skills/sim-output/SKILL.md`
- `CLAUDE.md`
- `docs/output/SpeciesMesoSpatial-h5.md`

**Verification Step:**

Run:
```bash
grep -l "docs/output/README.md\|README.md" .claude/skills/sim-output/SKILL.md CLAUDE.md docs/output/SpeciesMesoSpatial-h5.md
```

Expected:
All three files are listed.

**Notes:**

Add only the link line; change nothing else.
