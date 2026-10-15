# Ticket 02: species-time-grid

**Status:** ✅ Done

## Local Test Result

Subagent's run of the ticket's literal Verification Step command (`/scheduler/endTime 1 ms`, mesoscopic stage left at its default ON) passed (exit 0, no EEEE/FatalException) but only showed 12 distinct recorded times in Species.Txt, not 19 — it correctly flagged this as a pre-existing interaction with the mesoscopic hand-over (default 5 ns), not a defect in its source change: `ScoreSpecies::EndOfEvent` stops recording past the hand-over once the mesoscopic stage is active, so only grid points at/below 5 ns are ever reached regardless of the `AddTimeToRecord` list.

This session re-ran verification with `/chem/meso/enable false` and `/scheduler/endTime 1 us` (so the full particle-based IRT stage runs to 1 us and every grid point is reachable): `cmake --build build --config RelWithDebInfo --target sim` (via vcvars64, up to date), then `./sim.exe scratch_timegrid_smoke4.in --dir .../ticket02-smoke4` — exit 0, "The simulation took: 126.97 s", 0 occurrences of `EEEE`. `grep -E '^[0-9.]+$' Species.Txt` lists exactly 19 distinct time blocks: 0.001 ns through 999.999 ns (1 ps to 999999 ps), matching the smallest/largest bounds unchanged from before this ticket.

## Review Notes

Reviewed `git diff -- src/scoring/ScoreSpecies.cc` against the Acceptance Criteria: the constructor's `AddTimeToRecord` calls now list exactly the 19 specified picosecond values (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 999999), replacing the old 7-entry list; first/last values (1 ps, 999999 ps) unchanged; no other line in the file touched. All 3 acceptance criteria hold once verified with a macro that doesn't let the mesoscopic hand-over truncate recording (see Local Test Result) — the ticket's own Verification Step command needed that adjustment, not the source change.

## Blockers / Challenges

The ticket's prescribed Verification Step macro (meso ON, default 5 ns hand-over) can never observe more than ~12 of the grid's time points regardless of how many `AddTimeToRecord` calls exist, since `ScoreSpecies` stops recording species past the hand-over by design. Re-verified instead with `/chem/meso/enable false` + explicit `/scheduler/endTime 1 us`, confirming the source change itself is correct. Also: the first verification attempt used `/scheduler/endTime 1 ms` with meso disabled, which runs the full (slow, by design) IRT particle-based stage 1000x longer than needed and had to be killed and re-run at 1 us.

## Commits

- f2216e1 feat: finer log-spaced species time-recording grid (ticket 02)

## Time Spent

16m (ticket-start.js to ticket-complete.js)
