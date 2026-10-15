# Ticket 06: smoke-matrix

**Status:** ✅ Done

## Local Test Result

12 runs (SBS / IRT_syn meso off / IRT_syn + meso, each with O2 21 % and water, Serial and --threads 4; 10 keV e-, 2 events, end time 1 us) plus 2 extra SBS runs under BoscoloChem and Tonneau2025 (100 ns): all exit 0 after the fix below. Wall time 8-49 s. Only warning: the known benign WrongResolution lines in the 4 meso runs. Species.* and Reactions.* in every run, SpeciesMeso.* only in meso runs; results-root Manifest.json index and sub_01/Manifest.json present with correct timeStepModel, mesoEnabled, chemistry, O2 entry, runMode/threads.

e_aq at the last time, O2 vs water (2 events summed; meso rows per-event mean): SBS 2/4 vs 201/182; IRT_syn 6/3 vs 286/289; meso 1/1.5 vs 167/146.5. O2- is high with O2 in every mode. No contradiction. Full table: .scratch/tests/.../06-smoke-table.md (not committed).

## Review Notes

Defect found and fixed: `ScoreSpecies::EndOfEvent` cut Species.Txt at the hand-over time (5 ns) even when the mesoscopic stage was off (SBS, or IRT_syn with `/chem/meso/enable false`), so those modes had no records after 1 ns. It now also requires `MesoSettings::StageEnabled`. Reviewed the one-condition diff; no unit test possible (needs a live Geant4 kernel), covered by the matrix re-run. Observation, not changed: with an end time below the last grid time, Species.Txt still lists later record times with the end-of-chemistry counts (pre-existing).

## Blockers / Challenges

None.

## Commits

- acfa422 fix(scoring): keep Species records past 5 ns when the mesoscopic stage is off (ticket 06)

## Time Spent

14m (ticket-start.js to ticket-complete.js)
