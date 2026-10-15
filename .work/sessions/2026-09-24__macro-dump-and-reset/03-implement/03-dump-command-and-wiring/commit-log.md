# Ticket 03 Implementation

**Status:** ✅ Done

## Commits

- `960c955` feat: add /run/dumpDataAndReset macro command with exit-time safety net

## Local Test Result

Build (`sim`, RelWithDebInfo):
```
[1/3] Building CXX object CMakeFiles\sim.dir\src\RunAccumulatorMessenger.cc.obj
[2/3] Linking CXX executable sim.exe; Copying macro files to output directory...
```

Manual verification, scenario 1 (two beamOns + explicit dump + third beamOn
relying on the exit-time safety net):
```
[RunAccumulatorMessenger] dumped and reset (prefix='run1_')
[RunAccumulatorMessenger] dumped and reset (prefix='EndOfRun_')
```
`run1_Species.Txt`/`Reactions.Txt`/`EnergyDeposit.Txt` (100 keV, 4 events) and
`EndOfRun_*` (50 keV, 2 events) all present, non-empty, content proportional
to event count. No `FatalException`/`EEEE` in the log. All 8 CSV/Txt files
present with correctly prefixed names and no `_bis` renaming (see Review
Notes for why this required a code change beyond the ticket's literal spec).

Manual verification, scenario 2 (prefix reuse):
```
*** G4Exception : DuplicateDumpPrefix
      issued by : RunAccumulatorMessenger::SetNewValue
prefix 'dupe_' was already used earlier in this run -- choose a different prefix
*** Fatal Exception *** core dump ***
```
Process aborted as expected; no output written for the refused dump.

Full unit suite re-run after the fix below, unaffected (this ticket adds no
new unit tests, matching project convention for messengers):
```
100% tests passed, 0 tests failed out of 6
```

## Review Notes

**Deviation from the ticket's literal code, found during manual
verification, not anticipated in planning:** the first verification run
(exactly as specified) showed a real defect -- the second
`/run/dumpDataAndReset` call in a process (here, the `EndOfRun_` exit-time
flush, following an explicit `run1_` dump) hit a Geant4 `Analysis_W001`
warning ("Ntuple filename ... already in use") from
`G4CsvFileManager::CreateNtupleFile`, and silently produced no
`EndOfRun_Species_nt_species.csv` at all (data loss) while renaming
`EndOfRun_Reactions_nt_reactions.csv` to `_bis` unexpectedly.

Root cause, traced through the Geant4 11.4.1 analysis-manager source: each
`OpenFile()`/`CreateNtuple()`/`Write()`/`CloseFile()` cycle registers the
resolved ntuple filename in `G4TFileManager<FT>::fFileMap`, but
`CloseFiles()` explicitly does **not** clear that map (the source has a
comment: `// As the files were set to nullptr, clear should not be needed` /
`// fFileMap.clear();`, left commented out) -- so a later cycle deriving a
different name from a different prefix should be fine, but empirically
wasn't (the exact mechanism inside `GetNtupleFileName`'s per-ntuple-object
"cycle" versioning wasn't fully traced). `analysisManager->Reset()` (called
as a first attempt) does **not** touch this map either -- it only resets
`fVNtupleFileManager` (ntuple bookings), confirmed by reading
`G4ToolsAnalysisManager::ResetImpl()`. `analysisManager->Clear()` does --
it calls `fVFileManager->Clear()`, which for `G4VTFileManager<FT>` is
`G4TFileManager<FT>::ClearData()` (`fFileMap.clear()`) plus
`G4BaseFileManager::ClearData()`. Added one `analysisManager->Clear()` call
in `RunAccumulatorMessenger::DumpAndReset`, right after the Reactions CSV
block, once per dump cycle. Re-verified: both scenarios above now pass
clean, with zero `Analysis_W001` warnings and no `_bis` files. `Clear()`
also resets histogram/profile data, which this project doesn't use, so
this is safe.

This changed the CLAUDE.md wording I wrote in Step 6 versus the ticket's
literal text: I added one sentence documenting the `Clear()` call and why
it's necessary (the ticket's own drafted CLAUDE.md paragraph didn't
anticipate this, since it was written before verification surfaced the
issue). No other deviations -- `RunAccumulator.hh`, the messenger's
constructor/`SetNewValue`/`GetCurrentValue`/`FlushIfPending`, `RunAction.cc`,
and `sim.cc` all match the ticket's code verbatim.

## Time Spent

~1.5 hours (most of it tracing the Geant4 analysis-manager source to find
the correct fix after the first verification attempt surfaced data loss).

## Blockers / Challenges

The `Analysis_W001`/data-loss issue above was a genuine blocker until
diagnosed and fixed -- not anticipated by the ticket's plan, which assumed
(incorrectly) that the prefix-uniqueness guard alone would prevent any
`_bis`-style collision. It doesn't: that guard only stops *literal* prefix
reuse; the Geant4-internal collision could still occur between two *different*
prefixes without the `Clear()` fix. Resolved by adding the `Clear()` call
per Review Notes above; re-verified with both manual scenarios.

Build required the MSVC x64 environment via `vcvars64.bat`, same as
tickets 01/02. Long-running `sim.exe` invocations (each ~3-5 min real time
for the chemistry stage) needed background execution and polling via the
Monitor/background-task mechanism rather than a single synchronous call.

## Token Usage

- **Input:** 168
- **Output:** 54809
- **Cache read:** 27724112
- **Cache creation:** 91607
- **Total:** 27870696
