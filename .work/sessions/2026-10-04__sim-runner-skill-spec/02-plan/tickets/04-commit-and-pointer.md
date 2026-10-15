# Ticket 04: commit-and-pointer

**Model:** haiku

**Acceptance Criteria:**
- [ ] dotfiles branch `docs/g4run-spec` holds the SPEC.md commits and nothing else; user's uncommitted dotfiles changes untouched.
- [ ] dnachem-min `CLAUDE.md` § Build and Run has one line pointing to `~/dotfiles/claude/skills/g4run/SPEC.md`.

**Files to Touch:**
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
git -C ~/dotfiles log --stat main..docs/g4run-spec --format=%s; git -C ~/dotfiles status --short; grep -n g4run CLAUDE.md
```

Expected:
Only SPEC.md in the branch commits; the same pre-existing modified files still listed as modified; one CLAUDE.md line.

**Notes:**

No push and no PR in dotfiles unless the user asks.
