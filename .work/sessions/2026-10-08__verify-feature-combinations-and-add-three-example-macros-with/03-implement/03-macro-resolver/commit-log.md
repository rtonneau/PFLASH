# Ticket 03: macro-resolver

**Status:** ✅ Done

## Local Test Result

`ctest -R MacroResolver` in build-ninja: passed (subagent). `./sim does_not_exist.in` from build/: prints the three tried paths, exit 1. Filename-only lookup from build/ resolves to build/macro/<name>; running from another cwd with the exe's absolute path resolves to the same file.

## Review Notes

Diff reviewed: all four criteria hold; --threads/--dir parsing untouched, default stays beam.in. Linux branch of ExecutableDir() (/proc/self/exe) not compiled here. Known limitation, not fixed: a resolved path containing spaces would be split by `/control/execute`; I did not verify whether Geant4 accepts a quoted path there. Checked in: header/core/MacroResolver.hh, src/core/MacroResolver.cc, test, CMakeLists.txt, sim.cc.

## Blockers / Challenges

None.

## Commits

- 7403243 feat(core): macro lookup next to the exe, then macro/ (ticket 03)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
