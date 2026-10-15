# Ticket 03: rename-skill-and-living-docs

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 0.96)
**Effort:** medium

**Acceptance Criteria:**
- [ ] `.claude/skills/sim-output` is `.claude/skills/pflash-output`, its frontmatter name and body say `pflash`.
- [ ] `docs/recipes/dnachem-min-macro-command-reference.md` is `pflash-macro-command-reference.md`, and every link to it is updated.
- [ ] `dnachem-min` becomes `PFLASH` in recipes, `docs/output/**`, `docs/visual/*.svg`, `docs/agents/*`, headers and sources (comments), CMakeLists.txt comments and `.claude/*`.
- [ ] Kept history files are unchanged: `docs/adr/*`, `CHANGELOG.md` entries, `docs/reviews/*`, `docs/literature-comparison/**`.

**Files to Touch:**
- `.claude/skills/sim-output/SKILL.md`, `docs/recipes/*`, `docs/output/**`, `docs/visual/*.svg`, `docs/agents/*`
- `header/**/*.hh`, `src/**/*.cc` (comments only), `CMakeLists.txt`

**Verification Step:**

Run:
```bash
grep -rIl "dnachem-min\|sim-output" --exclude-dir=.git --exclude-dir=.work . ; git diff --stat -- docs/adr CHANGELOG.md docs/reviews docs/literature-comparison
```

Expected:
Only kept history paths (ADRs, CHANGELOG, review, literature-comparison) and CLAUDE.md/README/CONTEXT.md (ticket 5) are listed; the diff stat for kept history is empty.

**Notes:**

Check how the skill is referenced in CLAUDE.md, `.claude/settings.json` and the diagrams. Use word-bounded replacement and review the diff; comments only in code, never identifiers. `docs/literature-comparison/*` is listed in the grep because it keeps `dnachem-min` as history.
