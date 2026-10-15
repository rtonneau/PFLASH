# Ticket 02: end-time-default-and-manifest-mode

**Status:** ✅ Done

## Local Test Result

Four sim runs by the subagent (fresh --dir each, exit 0): SBS -> chemistryEndTime_ns 1000, chemistryModel SBS, 0 SpeciesMeso files; IRT_syn + `/chem/meso/enable false` -> 1000, IRT_syn, 0 files; SBS + `/scheduler/endTime 500 ns` after initialize -> 500; IRT_syn meso on + `/scheduler/endTime 1 ms` -> 1e6 ns, IRT_syn+mesoscopic, 2 SpeciesMeso files, old keys plus `timeStepModel` and `mesoEnabled`. RunAccumulatorTest passes under ctest. The 1 s meso-on default was not run (slow); the code path is the unchanged value.

## Review Notes

Diff reviewed against the four criteria; all hold. In Serial mode Build() runs before the macro, so the same default is also set in `DnaChemistryList::CheckTimeStepModel` (at /run/initialize), as with the old hard-coded call a `/scheduler/endTime` before initialize is overwritten. SpeciesMesoSpatial.h5 is not produced with meso off (nothing stages it). No changes after review. No unit test for the model key: the manifest tree is built in Geant4-facing code.

## Blockers / Challenges

None.

## Commits

- 20e9373 feat(chem): end-time default and mode in manifest, no SpeciesMeso without meso (ticket 02)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
