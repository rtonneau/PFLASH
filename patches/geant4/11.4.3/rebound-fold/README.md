# rebound-fold (Geant4 11.4.3)

`rebound-fold.patch` fixes `G4ChemReboundTransportation::calculateNextCoordinate`
(`source/processes/electromagnetic/dna/processes/src/G4ChemReboundTransportation.cc`),
issue 3 of `docs/dose-rate-pulse-study/geant4-issues.md`.

The stock code reflects a coordinate only once, so it returns a point outside the
box when a step overshoots a wall by more than the box length, and it maps a point
on the far wall to the near one. Long late IRT_syn steps in a few-um reflecting box
(dose-rate study) hit both cases. The patch folds with period `2L`:

```
folded = fmod(x - low, 2L) (made non-negative)
result = low + (folded <= L ? folded : 2L - folded)
```

and raises `ChemRebound001` (`FatalErrorInArgument`) for a non-finite input.

Demonstration: `test/dose-rate-patches/ReboundFoldFixture.cc`, run by
`test/dose-rate-patches/RunSufficiency.ps1 -Patch 3` (stock: 2750 / 100000 steps
outside the box; patched: 0).

Apply from the Geant4 source root: `git apply rebound-fold.patch` (LF line endings).
