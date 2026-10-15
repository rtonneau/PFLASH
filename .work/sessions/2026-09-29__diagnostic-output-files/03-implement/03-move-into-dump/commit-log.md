# Ticket 03 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- feat: move pre-chemical files into the dump and manifest (F-002, ticket 03)

## Local Test Result

```
Serial (t3s): d1/ holds PreChemical_run0_event0, run0_event1, run1_event0; Manifest files lists them in order; .pending_prechem empty; no EndOfRun_ files.
MT (t3m, 2 threads): mt_PreChemical_run0_event0..3 present, all in mt_Manifest.json; .pending_prechem empty; no PreChemicalMoveFailed / EEEE.
ctest build-ninja: 100% passed, 0 failed out of 10.
```

## Review Notes

Change confined to WriteAllAndReset plus docs (CLAUDE.md, project json, sim-output skill). No output_event mention remains.

## Time Spent

~0.3 hours

## Blockers / Challenges

None

## Token Usage

- **Input:** 26
- **Output:** 3162
- **Cache read:** 878192
- **Cache creation:** 43705
- **Total:** 925085
