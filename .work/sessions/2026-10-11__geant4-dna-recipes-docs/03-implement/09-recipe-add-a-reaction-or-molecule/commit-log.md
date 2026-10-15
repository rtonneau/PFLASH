# Ticket 09: recipe-add-a-reaction-or-molecule

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/add-a-reaction-or-molecule.md && grep -q "PureWaterReactions" docs/recipes/add-a-reaction-or-molecule.md && grep -q "add-a-new-chemistry.md" docs/recipes/add-a-reaction-or-molecule.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/add-a-reaction-or-molecule.md`, grounded in `PureWaterReactions.cc` (the `add`/`partial` lambdas, bulk list, `SetReactionType`/`PartialReactionsEnabled` gating) and `Tonneau2025Reactions.cc`'s `ConstructTonneau2025Molecules` for the Extra Molecule pattern, including the idempotence requirement (correctly notes the hook runs twice per process in the normal case: once from `ConstructMolecule`, once from `/chem/select`). Links `add-a-new-chemistry.md`'s ticket-8 Serial-vs-MT finding instead of re-deriving it, and ADR 0002/0007 instead of repeating them. No changes needed.

## Blockers / Challenges

None.

## Commits

- 0783ecc docs: add-a-reaction-or-molecule recipe (ticket 09)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
