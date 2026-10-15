# Ticket 02 Implementation

**Status:** ✅ Done

## Commits

- `b26a262` feat(chem): IRT_syn particle stage hands over to the mesoscopic model (ticket 02)

## Local Test Result

```
Inline re-run (02b-spike.in: Debug, end time 1 s, 10 keV e-, beamOn 2, PureWater, 1 mm box):
  Serial:     exit 0, dumped and reset, 139.4 s (Debug logging), 0 EEEE/WWWW, 0 DRIFT lines,
              mesh 65536 -> 32 pixels, last mesh ends at 1.00002e9 ns, total 768
  --threads 2: exit 0, dumped and reset, 71.9 s, 0 EEEE/WWWW, 0 DRIFT lines, same mesh sequence
Subagent (Info level): ~2.9 s/event to 1 us, ~11.3 s/event to 1 s (IRT_syn to 5 ns ~1.35 s, meso ~9.9 s)
```

## Review Notes

- Implemented in two subagent attempts on **opus**. The inline follow-up reviewed the diff and re-ran both spike runs. **No changes** to the code.
- **Attempt 1 BLOCKED.** It started at 131072 pixels on the 1 mm box and hit a 32-bit overflow in `G4DNAMesh::ConvertIndex` at the first coarsening (fatal `G4DNAMesh013`). The user decided to keep the 1 mm box and cap the mesh at 65536 cells per side (15.26 nm cells). The ticket and ADR 0006 were amended (`f531002`).
- **Attempt 2 READY:** 11 mesh changes per event (65536 → 32 pixels), with molecules conserved through the hand-over and every change.
- **Geant4 integration fixes kept:**
  - The molecule counter is muted from the hand-over until `EndProcessing`. This avoids `TIME_DONT_MATCH`: `Voxelizing` kills the tracks, but they are only deleted after the scheduler clock has jumped.
  - A record-time grid is registered, because `RecordTime` dereferences an uninitialised iterator otherwise.
  - `ParticleBasedCounter` is called as in UHDR.
  - `AddTimeStep` was removed: the IRT_syn stepper ignores it.
- **Consequence:** `Species.*` holds frozen hand-over counts after 5 ns. Tickets 05 (truncate `Species.*` at the hand-over) and 07 (read G(1 µs) from `SpeciesMeso.csv`) were amended.
- **Pending:** `macro/beam.in` still has `/process/chem/TimeStepModel SBS` and is now fatal. Ticket 06 removes it.

## Time Spent

~0.6 hours (two subagent attempts + reviews)

## Blockers / Challenges

- **Attempt 1:** the Geant4 int overflow, resolved by a user decision (mesh cap).
- **Attempt 2:** the `TIME_DONT_MATCH` from the molecule counter, fixed by muting it until the end of the event.

## Token Usage

- **Input:** 290
- **Output:** 29212
- **Cache read:** 23209026
- **Cache creation:** 356177
- **Total:** 23594705
