# Ticket 01: scaffold-recipes-index

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] `docs/recipes/` directory exists.
- [ ] `docs/recipes/README.md` exists and lists exactly these 9 links, each with a one-line description: `create-a-scorer.md`, `create-a-species-counter.md`, `mesoscopic-3d-species-counting.md`, `export-data-as-hdf5.md`, `create-a-macro-command.md`, `dnachem-min-macro-command-reference.md`, `add-a-new-chemistry.md`, `add-a-reaction-or-molecule.md`, `add-a-per-run-counter.md`.
- [ ] README states the purpose of the directory (seed material for future Claude skills) and the fixed section skeleton every recipe follows (What this is / Generic pattern / dnachem-min reference implementation / Serial vs MT / See also), matching the factual, cross-referencing style of `docs/output/README.md`.

**Files to Touch:**
- `docs/recipes/README.md`

**Verification Step:**

Run:
```bash
grep -c "](.*\.md)" docs/recipes/README.md
```

Expected:
`9`

**Notes:**

This is pure scaffolding — the 9 linked files do not need to exist yet (later tickets create them with these exact names). Keep the README itself short; it is an index, not a recipe.
