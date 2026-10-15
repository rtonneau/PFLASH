# Ticket 03 Implementation

**Status:** ✅ Done

## Commits

- 5d3a5bc feat: select the chemistry with /chem/select (ticket 03)

## Local Test Result

Run from `build` (RelWithDebInfo) with scratch macros in `build/macro/`, outputs under `.scratch/tests/2026-09-28__multiple-dna-chemistry/`:

```
/chem/list run (gps_list.in):      exit=0
  Available chemistries: PureWater (default)
/chem/select Nope (gps_select_unknown.in): fatal, exit=127
  *** G4Exception : InvalidChemistrySelection
  Unknown chemistry 'Nope'. Valid names: PureWater.
default run (gps_dump.in):          exit=0  DEFAULT_SAME  EDEP_SAME  markers 2  failures 0
/chem/select purewater (lower):     exit=0  LOWER_SAME    markers 2  failures 0
Info line "chemistry = PureWater":  1 in each log
```

`DEFAULT_SAME` / `LOWER_SAME`: sorted reaction dump equals the ticket-02 baseline (captured before any refactor edit).

`PHYS_SAME` is not asserted: as documented in the ticket 02 log, `PhysicsInteractions.csv` differs between runs of the same binary. Counts here are in the same range as the earlier runs.

Extra checks from the plan's risk list:
- `/control/manual /chem/` lists both `/chem/select` and `/chem/list` (the plain `G4UImessenger` attaches to the existing `/chem/` directory without a duplicate).
- `/chem/select PureWater` after `/run/initialize` is refused: `***** Illegal application state </chem/select PureWater> *****`.

Unit tests, `build-ninja` (Debug), all targets built first:

```
100% tests passed, 0 tests failed out of 7
```

## Review Notes

- Implemented as the ticket specifies. Two-name conflict via macro is not run end to end here (needs a second Chemistry); it is covered by the `TestSelectDifferentNameConflicts` unit test and gets a macro check in ticket 04.
- `~DnaChemistryList()` moved to the `.cc` so the `unique_ptr<ChemistrySelectMessenger>` sees the complete type.
- Dropped the now-unused `PureWaterReactions.hh` include from `DnaChemistryList.cc`; updated file and class comments to say the reaction content comes from the selected Chemistry.
- clangd flags `#include "globals.hh"` in `BuiltInChemistries.cc` as unused; it is needed for `G4Exception`/`FatalException`, so it stays.
- Staged only the six ticket files; `compile_commands.json` and the hook log were left out.

## Time Spent

~0.75 hours

## Blockers / Challenges

None. One tool call failed because the shell was still in `build/macro` (relative build path); re-ran with an absolute path.

## Token Usage

- **Input:** 22
- **Output:** 10019
- **Cache read:** 2431988
- **Cache creation:** 16909
- **Total:** 2458938
