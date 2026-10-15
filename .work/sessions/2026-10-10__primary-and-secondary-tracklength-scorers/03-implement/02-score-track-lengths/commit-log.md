# Ticket 02: score-track-lengths

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja --output-on-failure`: 17/17 passed (re-run in session).
Smoke macro `.scratch/tests/.../smoke_tl.in` (10 keV e-, end time 1 ms, beamOn 2, dump to `tl/`), Serial and `--threads 2`: each `TrackLengths.csv` has 2 rows, `stopped`, Ekin0 10 keV, Ekin end 0.000025 keV (the 25 meV solvation KE), primary 1838-2793 nm, first-gen 148-217 e- / 2.7-3.9 um, all-gen ~500 e- / 6.2-6.8 um. Both files are listed in `Manifest.json` `files`.

## Review Notes

Implemented by an opus ticket subagent (high effort). Reviewed the diff in session: thread-local kill flag set on both PrimaryKiller kill branches for track 1, reset in Initialize (per event); TrackingAction end-reason order killed > escaped > stopped; e- only for secondaries; EventAction resets at begin and commits only non-aborted events; Run live/owned pointer + Merge + per-run Clear; RunAccumulator and its test extended; dump writes and lists both files. No changes after review.

## Blockers / Challenges

sim.exe needs `E:\miniconda3\envs\qt5\Libraryin` first on PATH (0xC0000135 otherwise) and cannot run from Git Bash; vswhere failed in PowerShell, so vcvars64.bat from VS 18 Community was called directly. Environment only, no code impact.

## Commits

- 42d6f75 feat: score primary and secondary-electron track lengths per event (ticket 02)

## Time Spent

8m (ticket-start.js to ticket-complete.js)
