# Ticket 03 Implementation

**Status:** ✅ Done

## Commits

- 62629d7 feat: count physical interaction firings via SteppingAction

## Local Test Result

```
Full ctest suite (build-ninja, Debug) after this ticket's changes:
100% tests passed, 0 tests failed out of 5 (no regressions).

Smoke run: sim.exe smoke_energy.in --dir ../.scratch/tests/2026-09-23__physics-stage-output/task-03
(25 keV e-, /run/beamOn 10, /dnaLogger/verbose Info)

Exit code: 0

Log markers present:
[RunAction] species yields written (Species.Txt / Species_nt_species*.csv) for 10 recorded event(s)
[RunAction] reaction counts written (Reactions.Txt / Reactions_nt_reactions.csv / ReactionsMetadata.csv)
[RunAction] energy deposit written (EnergyDeposit.Txt)
[RunAction] physical interaction counts written (PhysicsInteractions.Txt / PhysicsInteractions.csv)
The simulation took: 439.19 s to run (real time)

No EEEE / FatalException.

.scratch/tests/2026-09-23__physics-stage-output/task-03/PhysicsInteractions.Txt:
e-_G4DNAAttachment    count = 218
e-_G4DNAElastic    count = 617935
e-_G4DNAElectronSolvation    count = 12523
e-_G4DNAExcitation    count = 1529
e-_G4DNAIonisation    count = 12701
e-_G4DNAVibExcitation    count = 70819

PhysicsInteractions.csv: header "label,count" + matching rows.
No "Transportation" row in either file (grep found none).
```

## Review Notes

Plausible physically: elastic scattering dominates by far for a 25 keV
electron slowing down in water, which matches expectation. Notably,
`e-_G4DNAElectronSolvation` was correctly picked up by the generic `"G4DNA"`
substring filter even though it wasn't one of the processes explicitly
enumerated in the design/ticket notes -- this validates the filter's
generality over a hardcoded process-name list, exactly the robustness
property the design intended.

## Time Spent

~25 minutes (including the ~7 minute smoke-run itself)

## Blockers / Challenges

None

## Token Usage

- **Input:** 48
- **Output:** 8358
- **Cache read:** 5577010
- **Cache creation:** 14878
- **Total:** 5600294
