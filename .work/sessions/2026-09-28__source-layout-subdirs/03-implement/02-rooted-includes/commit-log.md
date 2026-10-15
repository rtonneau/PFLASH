# Ticket 02 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- `8aaf7b1` refactor: root project includes at header/ and drop per-directory include path

## Local Test Result

```
rewrite-includes.js: rewrote 113 includes in 49 files
per-line check against HEAD~ (before commit): 113 changed lines, 0 unexpected, same line count in every file
committed diff: 113 added include lines, 0 non-include changed lines
src/actions/TrackingAction.cc: #include <actions/TrackingAction.hh> (angle brackets kept)
remaining bare project includes: none; CMakeLists.txt: no foreach left, project_include_dirs = ${PROJECT_SOURCE_DIR}/header
clean rebuild: build (sim, 34 steps) OK; build-ninja (7 test targets) OK; no compiler errors
ctest: 100% tests passed, 0 tests failed out of 7
verify.sh after_rooted:
  water: checked 48, skipped 94 (<400), worst 8.5% [species:e_aq^-1@999.999], energy deposit equal
  boscolo: checked 48, skipped 94 (<400), worst 4.6% [species:e_aq^-1@999.999], energy deposit equal
  STATISTICALLY_COMPARABLE
```

## Review Notes

Verification is the statistical comparison agreed in ticket 01 (10 events at 10 keV, within 15% of the mean of three baselines, quantities with reference count of at least 400), not byte-identical output; see ticket 01's notes.

Git's pre-commit diff stat reported 115 changed lines while the script and the per-line check both said 113. The committed diff shows exactly 113 include lines changed, 0 others, so the 113 figure is the right one; the 115 stat was not investigated further.

`git` prints "LF will be replaced by CRLF" warnings for 11 files touched by the rewrite. Those files were already LF in the working tree before the rewrite (the script preserves whatever line endings a file has), so this is pre-existing mixed line endings in the tree, not something this ticket introduced.

## Time Spent

~0.7 hours (script, rewrite and diff checks, two clean rebuilds, verification run).

## Blockers / Challenges

None.

## Token Usage

- **Input:** 22
- **Output:** 5334
- **Cache read:** 1950825
- **Cache creation:** 11187
- **Total:** 1967368
