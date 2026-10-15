# Ticket 05 Implementation

**Status:** ✅ Done

## Commits

- 2b85253 docs: document named chemistries and /chem/select (ticket 05)

## Local Test Result

Both trees built (`build-ninja` all targets, `build` `sim`), exit 0:

```
100% tests passed, 0 tests failed out of 7      (ctest, build-ninja, Debug; none Not Run)
```

Smoke runs from `build` (10 keV e-, `/run/beamOn 2`):

```
gps_dump.in   (default Chemistry):   exit=0  success markers 2  failures 0  DEFAULT_SAME (sorted dump == baseline)
beam_boscolo.in (BoscoloChem):        exit=0  success markers 2  failures 0  "chemistry = BoscoloChem" x1
git diff --stat -- macro/beam.in:     empty (unchanged)
git status: only intended files (CLAUDE.md, README.md, header/PureWaterReactions.hh) plus two hook-modified
            files (.claude/hooks/hook-posttooluse.log, compile_commands.json) that were left out of the commit
```

Output plausibility (1 ps): default `°OH 961` / `H3O+ 795`; BoscoloChem run (`EndOfRun_Species.Txt`) `°OH 961`, `H2O2 26`; `EnergyDeposit` 20 keV in both; `Reactions.Txt` populated (114 lines).

Note on the BoscoloChem output names: `macro/beam_boscolo.in` does not issue `/run/dumpDataAndReset` (same as `beam.in`), so the safety-net flush at exit writes the files with the `EndOfRun_` prefix, and no `/chem/reaction/dump` file is produced. Both are expected. The reaction-dump equality for BoscoloChem was verified in ticket 04 with a scratch macro that dumps.

`PhysicsInteractions.csv` equality was not asserted (see the ticket 02 log: it varies run to run for the same binary).

## Review Notes

- `CLAUDE.md`: rewrote the `DnaChemistryList` Key Files entry, extended the `PureWaterReactions` entry, added entries for `BoscoloChemReactions`, `ChemistryRegistry` (+ `ChemistryTypes.hh`), `BuiltInChemistries`, `ChemistrySelectMessenger`, added `/chem/select`/`/chem/list` and `beam_boscolo.in` to the Macro section, and kept the ADR 0001 statement scoped to `PureWater`.
- `README.md`: `beam_boscolo.in` in the run list, a "Choosing a chemistry" subsection under Chemistry, repo layout and further-reading updates (ADR 0002, CONTEXT.md).
- Source headers for `DnaChemistryList.hh/.cc` were updated in tickets 02 and 03; `PureWaterReactions.hh` updated here to name it the default Chemistry and the reference to copy.
- One deviation from the plan text: the ticket's verification block reads `Species.Txt`/`Reactions.Txt` from the BoscoloChem run dir; they live under the `EndOfRun_` prefix for the reason above.

## Time Spent

~0.5 hours

## Blockers / Challenges

None.

## Token Usage

- **Input:** 16
- **Output:** 7513
- **Cache read:** 2029351
- **Cache creation:** 18069
- **Total:** 2054949
