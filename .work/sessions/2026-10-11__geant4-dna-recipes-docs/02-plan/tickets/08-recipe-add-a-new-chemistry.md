# Ticket 08: recipe-add-a-new-chemistry

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/add-a-new-chemistry.md` documents the steps to add a new Chemistry: copy a `catalog/` builder (e.g. `src/chemistry/catalog/PureWaterReactions.{hh,cc}`), implement its `BuildXxxReactions`/`BuildXxxBulkReactions` functions, register it in `src/chemistry/BuiltInChemistries.cc`, select it via `/chem/select <name>`, each step verified against the actual current source of those three files.
- [ ] States the `ChemistryRegistry` contract verified against `src/chemistry/ChemistryRegistry.cc`: name → builder-function-pointer registration, case-insensitive `Select`, idempotent `Register()`, and what makes an unknown or duplicate-with-different-name selection fatal.
- [ ] Includes a `## Serial vs MT` section verified against source: whether `ChemistryRegistry`'s registration/selection state is process-wide or re-established per worker thread (check whether `DnaChemistryList`'s constructor, which calls `BuiltInChemistries::Register()`, runs once per process or once per thread in MT, and what that implies for a Chemistry author) — state the confirmed answer with a citation, flagging explicitly if this needs checking against the Geant4 kernel's per-thread user-class instantiation behavior.
- [ ] Links `docs/adr/0002-named-chemistries.md` instead of repeating its rationale.

**Files to Touch:**
- `docs/recipes/add-a-new-chemistry.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/add-a-new-chemistry.md && grep -q "BuiltInChemistries" docs/recipes/add-a-new-chemistry.md && grep -q "ChemistryRegistry" docs/recipes/add-a-new-chemistry.md && echo OK
```

Expected:
`OK`

**Notes:**

Read `src/chemistry/ChemistryRegistry.cc` and `src/chemistry/BuiltInChemistries.cc` in full; `BuiltInChemistries::Register()`'s idempotency is explicitly relied upon elsewhere in the codebase (it is called from the `DnaChemistryList` constructor before any macro can select a Chemistry) — the MT question is specifically whether that constructor runs per-thread.
