# Ticket 03: extra-molecules-hook

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja --output-on-failure` after building all targets (incl. sim.exe): 14/14 passed, none Not Run. Re-run by the session.

## Review Notes

Implemented by a sonnet/high subagent; reviewed in this session against the diff. Nullable fourth field in the Chemistry struct (existing brace inits unchanged), call in ConstructMolecule after the H2O configuration, two registry tests, ADR 0007 (relaxes 0002). ChemistryRegistry.cc and BuiltInChemistries.cc needed no edit. sim.exe was built but not run (no Chemistry uses the hook yet; ticket 04 does the smoke run). No changes after review.

## Blockers / Challenges

None

## Commits

- fbbfeb2 feat: optional per-Chemistry constructMolecules hook, ADR 0007 (ticket 03)

## Time Spent

1m (ticket-start.js to ticket-complete.js)
