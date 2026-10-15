# Ticket 07: recipe-dnachem-min-macro-command-reference

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/dnachem-min-macro-command-reference.md && grep -q "/chem/meso" docs/recipes/dnachem-min-macro-command-reference.md && grep -q "/primaryKiller" docs/recipes/dnachem-min-macro-command-reference.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) grepped the whole `src/` tree for every messenger-construction pattern and read each matched file in full before writing. Enumerates every custom command grouped by directory with grep-confirmed state(s) and source line, plus the two built-in commands this project relies on. Found two genuine, verified anomalies while enumerating: `/dnaLogger/verbose` allowed mid-run (PreInit/Idle/GeomClosed/EventProc) and `/primaryKiller/*`'s five commands having no `AvailableForStates` call at all (default: every state). The Serial vs MT section links ticket 6's general rule rather than repeating it, then reports one real command-specific exception found during enumeration: `/chem/env/scavenger` calls `SetToBeBroadcasted(false)` (`ScavengerMessenger.cc:30`), so it is never replayed to worker threads, consistent with the scavenger world existing master-only. No changes needed.

## Blockers / Challenges

None.

## Commits

- ed0ddd1 docs: dnachem-min-macro-command-reference recipe (ticket 07)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
