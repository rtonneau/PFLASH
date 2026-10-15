# Ticket 01 Implementation

**Status:** ✅ Done

## Commits

- b54b92d refactor: rename acid-base list to bulk-reaction list (ticket 01)

## Local Test Result

```
ctest (build-ninja, Debug): 100% tests passed, 0 tests failed out of 7
smoke 01-t01 (10 keV e-, beamOn 2): dumped and reset; The simulation took: 32.83 s
diff <(sort 01-t01/reactions_dump.txt) 01-baseline.sorted:
  only the section header differs:
  < # Bulk reactions (acid-base buffer + scavengers)
  > # Acid-base reactions (bulk scavenger network)
cmp EnergyDeposit.Txt, PhysicsInteractions.csv vs 01-baseline: identical
grep -rn "AcidBase\|acid-base list" header src test: no hits
```

## Review Notes

- Identifier renames done mechanically with sed; doc comments reworded by hand ("acid-base buffer" kept where it names the chemistry itself).
- Rates and entries in both builders are unchanged (only the function names changed).
- The grill docs (ADR 0004, ADR 0001 note, CONTEXT.md) were already committed by the user in 76b9dfc, so this commit carries code only.

## Time Spent

~1 hour (including build-dir reconfiguration)

## Blockers / Challenges

- `build/` and `build-ninja/` had stale CMake caches pointing to the project's old location (`DEV/GEANT4/dnachem-min`), and `build-ninja` was RelWithDebInfo. Deleted only `CMakeCache.txt` + `CMakeFiles/` in both and reconfigured from `.claude-project.json` values (`build` RelWithDebInfo, `build-ninja` Debug, `G4_ROOT`, `DEV_DIR` vcpkg toolchain).
- Baseline smoke output was captured under `.scratch/tests/2026-09-28__o2-scavenger-uhdr/01-baseline/` (scratch dir), not `build/smoke/` as the plan said.

## Token Usage

- **Input:** 72
- **Output:** 19762
- **Cache read:** 7293553
- **Cache creation:** 46515
- **Total:** 7359902
