# Ticket 09: recipe-add-a-reaction-or-molecule

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/add-a-reaction-or-molecule.md` documents how a reaction or molecule is declared as plain data and wired into a Chemistry's builder, using `src/chemistry/catalog/PureWaterReactions.cc`'s `BuildPureWaterReactions`/`BuildPureWaterBulkReactions` as the reference (verified against its actual current source, including the `ChemistryTypes::SetReactionType`/`SetPartialReactionsEnabled` mechanism it depends on).
- [ ] Covers the "Extra molecule" pattern for a molecule that only exists under one Chemistry (HO3 under `Tonneau2025`), citing `docs/adr/0007-extra-molecules-per-chemistry.md` (link, don't repeat) and `src/chemistry/catalog/Tonneau2025Reactions.cc`/`Tonneau2025Table.cc`.
- [ ] Includes a `## Serial vs MT` section: links ticket 8's finding on whether Chemistry construction is per-thread or process-wide (since a reaction/molecule addition lives inside that same construction path) rather than re-deriving it.

**Files to Touch:**
- `docs/recipes/add-a-reaction-or-molecule.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/add-a-reaction-or-molecule.md && grep -q "PureWaterReactions" docs/recipes/add-a-reaction-or-molecule.md && grep -q "add-a-new-chemistry.md" docs/recipes/add-a-reaction-or-molecule.md && echo OK
```

Expected:
`OK`

**Notes:**

Depends on ticket 8 having already stated the Serial-vs-MT answer for Chemistry construction — write this ticket after ticket 8 so the link target's claim actually exists, even though the two files are otherwise independent.
