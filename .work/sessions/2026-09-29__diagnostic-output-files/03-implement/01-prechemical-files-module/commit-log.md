# Ticket 01 Implementation

**Status:** ✅ Done

## Commits

- `feat: add PreChemicalFiles staging/move helpers (ticket 01)` (single commit on fix/prechemical-output-files)

## Local Test Result

```
cmake --build build-ninja (all 10 test targets) && ctest --test-dir build-ninja --output-on-failure
 8/10 Test  #8: PreChemicalFilesTest ............   Passed    0.07 sec
100% tests passed, 0 tests failed out of 10
(all 10 tests Passed, none Not Run; log: .scratch/tests/2026-09-29__diagnostic-output-files/01-verify.log)
```

## Review Notes

- Standard library only; namespace `PreChemicalFiles`. File headers state portability and cite the ADR 0005 addendum.
- `MoveStaged` also creates the target's parent folder, so a dump subfolder need not exist yet (not in the spec, harmless).
- Numbers in staged names are limited to 9 digits to avoid int overflow.
- Added one extra test, `EnsureStagingDirCreates`.

## Time Spent

~0.3 hours

## Blockers / Challenges

None

## Token Usage

- **Input:** 20
- **Output:** 7819
- **Cache read:** 689808
- **Cache creation:** 75605
- **Total:** 773252
