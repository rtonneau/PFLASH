# Ticket 04: move-farokhi-macros

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.45)
**Effort:** low

**Acceptance Criteria:**
- [ ] All `macro/beam_farokhi_*.in` files live in `macro/farokhi2023/` (git mv).
- [ ] Macro references in docs, scripts (`macro/test_run.ps1`, `.work`-free), literature-comparison READMEs and CLAUDE.md use `farokhi2023/<name>.in`; the macro lookup order in the docs still reads correctly.
- [ ] A Farokhi macro starts from the build dir with `./pflash farokhi2023/<name>.in` (smoke only).

**Files to Touch:**
- `macro/beam_farokhi_*.in` → `macro/farokhi2023/`
- `docs/literature-comparison/farokhi2023/*`, `docs/recipes/*`, `CLAUDE.md`, `macro/test_run.ps1`

**Verification Step:**

Run:
```bash
grep -rn "beam_farokhi" --exclude-dir=.git --exclude-dir=.work . | grep -v "farokhi2023/beam_farokhi" | head
```

Expected:
No stray references without the `farokhi2023/` prefix (matches inside kept history may remain and should be fixed only where they are run instructions).

**Notes:**

`CMakeLists.txt` copies `macro/` with copy_directory, which is recursive, and `MacroResolver` tries `<exedir>/macro/<arg>`; a relative `farokhi2023/x.in` works. Macros that `/control/execute` other macros must be checked.
