# Ticket 04 Implementation

**Status:** ✅ Done

## Commits

- 7c747bd feat: write Manifest.json on every dump, remove EnergyDeposit.Txt (ticket 04)

## Local Test Result

```
Red: OutputDirTest -> C2039 'GetDirectory' is not a member of 'OutputDir'
Green: ctest --test-dir build-ninja --output-on-failure -> 100% tests passed, 0 tests failed out of 9

Smoke (build/, RelWithDebInfo), macro build/macro/manifest_smoke.in (untracked):
  10 keV x2 | 25 keV + /random/setSeeds 111 222 x1 | /run/dumpDataAndReset |
  10 keV x1 | /run/dumpDataAndResetToDir second | 10 keV x1 (never dumped -> EndOfRun_ flush)
Serial: exit 0, no EEEE/FatalException. m1/Manifest.json: 2 runs (10 keV/2 events seed 12345; 25 keV/1 event seed 111),
  totalEvents 3, totalEnergyDeposit_eV 45000. m1/second/Manifest.json: 1 run. m1/EndOfRun_Manifest.json: 1 run, prefix EndOfRun_.
MT (--threads 2): same runs/beam/totals, runMode "MT", threads 2; all 3 manifests valid JSON (python json.load);
  every name in "files" exists on disk; no EnergyDeposit.Txt anywhere.
```

## Review Notes

Smoke found a real bug: Species_nt_species_all.csv is only written when _ScoreSpecies_FOR_ALL_EVENTS is defined (it is commented out in ScoreSpecies.hh), so listing it unconditionally put a nonexistent file in `files`. Fixed with a matching #ifdef in RunAccumulatorMessenger; the MT run (built after the fix) confirms no missing files. The Serial m1 manifests predate the fix and still list that file. The sim-output skill doc claims that CSV is always written; ticket 05 corrects it.

Seed follows /random/setSeeds (111) but stays 111 for later runs: it is the engine's configured seed, as documented in ADR 0005, not a per-run evolving state.

GUI-mode leaves `macro` empty (SetMacroName is only called in the batch branch), as planned.

## Time Spent

~1 hour (two ~4 minute smoke runs)

## Blockers / Challenges

None

## Token Usage

- **Input:** 40
- **Output:** 13436
- **Cache read:** 4313257
- **Cache creation:** 27356
- **Total:** 4354089
