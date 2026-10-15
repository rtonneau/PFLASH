# Implementation Plan

**Session:** pflash-housecleaning-port-from-dnachem-min
**Date:** 2026-10-15T16:28:13.350Z
**Estimated effort:** 3-4 hours

## Strategy

Remove the dnachem-min working history first (it shrinks every later grep), then rename the executable and build files, rename the skill and living docs, move the Farokhi2023 macros, rewrite the scope documents, and finish with a whole-repo verification. No physics, chemistry or output behavior changes.

## Tickets Overview

- **Ticket 1:** Delete old sessions (via clean.js), retro/reads/scratch/changelog, old plans, .kb-notes, hook log, `report`, docs/cloud-work.
- **Ticket 2:** Rename executable `sim` to `pflash` (pflash.cc, CMake project/targets, .claude-project.json, build instructions) and check build + ctest.
- **Ticket 3:** Rename the `sim-output` skill to `pflash-output` and replace `dnachem-min` in living docs, comments, diagrams, recipes.
- **Ticket 4:** Move the Farokhi2023 macros to `macro/farokhi2023/` and fix every reference.
- **Ticket 5:** Rewrite CLAUDE.md scope, README and CONTEXT.md for PFLASH, add the origin note.
- **Ticket 6:** Whole-repo verification: grep for stale names, clean build, ctest, smoke run.

## Sequencing Rationale

Ticket 1 removes noise for later greps. Ticket 2 changes the binary name that tickets 3 and 5 document. Ticket 4 is independent of 3 but changes macro paths that docs reference, so it precedes the doc rewrite. Ticket 6 last.

## Risks & Mitigation

- **Risk:** deleting `.work/sessions` also deletes the current session → **Mitigation:** use gps `clean.js` with explicit ids, never the current one.
- **Risk:** the rename breaks build dirs holding stale CMake caches → **Mitigation:** reconfigure `build/` and `build-ninja/` from scratch in ticket 2.
- **Risk:** blind search-and-replace alters kept history (ADRs, CHANGELOG, review, literature write-ups) → **Mitigation:** exclude those paths explicitly and check with `git diff --stat`.
- **Risk:** macro subfolder breaks the macro lookup or build copy → **Mitigation:** run a Farokhi macro by `farokhi2023/<name>` in ticket 4 (copy_directory is recursive; confirm).

## Assumptions

- No behavior change; no pulse or dose-rate code in this session.
- Old history stays recoverable from git history and the original dnachem-min repo.
- The gps config and the current session stay untouched.
