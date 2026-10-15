# Ticket 03: spec-analysis-cli

**Model:** sonnet

**Acceptance Criteria:**
- [ ] Sections Analysis, Subcommands, Installation, Build milestones added.
- [ ] Analysis: `--runs` JSON schema, `--out`, optional `summary.json`, `analyses` row, generic scripts shipped with the skill, project registration `g4run.analysis.<name> {script, args, env}`, worked dnachem-min example (`compare_reference.py` on a sweep batch).
- [ ] Subcommands: enqueue, status, show, log, cancel, requeue, prune, analyze, analyses, dispatcher status|stop, each with syntax, flags, example, exit codes.
- [ ] Installation: venv bootstrap, SKILL.md structure, linking into `~/.claude/skills` via bootstrap.ps1 (described, not done).
- [ ] Build milestones: ordered list for the later implementation session.
- [ ] Committed in dotfiles (SPEC.md only).

**Files to Touch:**
- `~/dotfiles/claude/skills/g4run/SPEC.md`

**Verification Step:**

Run:
```bash
for c in enqueue status show log cancel requeue prune analyze analyses dispatcher; do grep -q "g4run $c" ~/dotfiles/claude/skills/g4run/SPEC.md && echo ok $c || echo MISSING $c; done
```

Expected:
`ok` for all ten subcommands.

**Notes:**

Analysis scripts behind reported numbers are committed in the project (see project memory); the spec should say the skill records script git hash.
