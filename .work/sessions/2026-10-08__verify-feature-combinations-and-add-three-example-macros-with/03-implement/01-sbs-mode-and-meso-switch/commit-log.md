# Ticket 01: sbs-mode-and-meso-switch

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja --output-on-failure`: 14/14 passed (re-run in this session). The subagent ran the SBS + `/chem/meso/enable false` macro (10 keV e-, 2 events, 10 ns end time): exit 0, log shows `time-step model = SBS, mesoscopic stage off`, no exception, reactions counted. Extra checks by the subagent: SBS + `enable true` fatal `MesoWithSbs`; IRT fatal `UnsupportedTimeStepModel`; IRT_syn default has meso on; IRT_syn + `enable false` completes; SBS + O2 21 % completes and produces O2-.

## Review Notes

Diff reviewed against the Acceptance Criteria: all five hold. The model check moved to `CheckTimeStepModel()` in `ConstructProcess()` (master) because in Serial mode `ConstructTimeStepModel()` runs at the first event. SBS user minimum time steps (from `b26a262^`) are added per thread in `StartProcessing()`. Extra header edits (`DnaChemistryList.hh`, `MesoMessenger.hh`, `TimeStepAction.hh`) are declarations only. No changes after review. Known leftovers for ticket 02: SBS still writes a header-only `SpeciesMeso.*` and the manifest still says `IRT_syn+mesoscopic`. Not committed: `compile_commands.json` and the hook log (tool noise).

## Blockers / Challenges

`sim.exe` needs the Qt5 conda `Library\bin` on PATH to start (DLL error 0xC0000135 otherwise); added to the run command only, no config changed.

## Commits

- e5cdbd5 feat(chem): SBS selectable and /chem/meso/enable switch (ticket 01)

## Time Spent

11m (ticket-start.js to ticket-complete.js)
