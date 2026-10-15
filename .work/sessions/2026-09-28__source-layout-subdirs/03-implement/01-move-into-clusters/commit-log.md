# Ticket 01 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- `c800509` refactor: cluster src/ and header/ into subdirectories (moves only)

## Local Test Result

```
git diff --cached -M100% --name-status: 64 R100, 0 other (plus CMakeLists.txt modified)
ls src/*.cc header/*.hh: no match
clean rebuild: build (sim, 34 steps) OK; build-ninja (7 test targets, 27 steps) OK
ctest: 100% tests passed, 0 tests failed out of 7
verify.sh after_moves:
  water: checked 48, skipped 94 (<400), worst 8.5% [species:e_aq^-1@999.999], energy deposit equal
  boscolo: checked 48, skipped 94 (<400), worst 8.5% [species:e_aq^-1@999.999], energy deposit equal
  STATISTICALLY_COMPARABLE
```

## Review Notes

Verification was amended mid-ticket by the user. The two-run baseline was not reproducible (same binary, different output: 7 of 9 water files and 2 boscolo files differ), so byte-identical output cannot be the pass condition. Decision: statistical agreement within 10-15% is enough.

Implemented as 10 events at 10 keV (instead of 2), three baseline runs, and a comparator (`.scratch/tests/2026-09-28__source-layout-subdirs/compare.js`). The comparator keys reactions by label (reaction IDs differ per run) and checks physics-process totals, per-reaction totals and species counts per time against the baseline mean, only where the reference count is at least 400. Below 400, same-code runs already exceed 15% from Poisson noise.

Calibration: the three baselines agree with each other within 12.6% worst case at min count 400, and a synthetic 20% drop in `Elastic` is detected. Tickets 01 and 02 were reworded accordingly; ticket 03 has no simulation check.

Unexplained: why the fixed seed (`kDefaultSeed = 12345` in `sim.cc`) does not give reproducible runs. Hypothesis, untested: address-dependent ordering in the chemistry stage. Worth a separate look.

## Time Spent

~1.5 hours (baseline diagnosis and calibration, moves, two clean rebuilds, verification).

## Blockers / Challenges

Baseline non-reproducibility (see Review Notes), resolved by the user's decision to accept statistical comparability. One transient `git mv` failure (`header/TimeStepAction.hh`: Permission denied, likely a file lock); the retry succeeded and all 64 renames are present.

## Token Usage

- **Input:** 74
- **Output:** 26456
- **Cache read:** 5801911
- **Cache creation:** 47316
- **Total:** 5875757
