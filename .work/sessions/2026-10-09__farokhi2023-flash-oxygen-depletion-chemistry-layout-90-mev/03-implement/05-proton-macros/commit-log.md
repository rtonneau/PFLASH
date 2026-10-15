# Ticket 05: proton-macros

**Status:** ✅ Done

## Local Test Result

Subagent wrote all 4 macro files correctly (confirmed on review: structure, header comments, and O2 molar values all match the Acceptance Criteria exactly). It then launched the 4 Verification Step runs in the background and monitored them for ~90 minutes: no errors, but each 90 MeV proton event was taking 40-70+ minutes (the 0pct run reached event 2/20, the other three were still on event 1), versus the ticket's "several minutes per file" estimate. This session killed the 4 still-running `sim.exe` processes (each ~5300s CPU time already spent, PIDs 9384/20928/26660/34572) rather than let them run for the many hours `/run/beamOn 20` would need at this rate.

## Review Notes

The macro files themselves satisfy every acceptance criterion as written and need no further changes. The ticket's underlying design assumption — `/chem/meso/enable false` + `/scheduler/endTime 1 us` is computationally tractable for a 90 MeV proton at the default 500 um halfBox, the way it was for the electron smoke tests in tickets 2/3/4 — is wrong. Those earlier tests used a point-like low/moderate-energy electron source; a 90 MeV proton traversing the full 1 mm box deposits roughly 570 keV spread along a ~1 mm path (LET ~0.57 keV/um), producing a far larger and more spatially spread radiolytic-species population for the particle-based IRT stage to resolve pairwise, with no mesoscopic hand-over to fall back on. This is a genuine scaling problem in the design the grill settled on (session 2026-10-09), not a bug in this ticket's files.

## Blockers / Challenges

BLOCKED. The 4 proton macros are computationally infeasible to verify (and to actually use for the paper's G-value reproduction) as specified: each event takes 40-70+ minutes, so `/run/beamOn 20` would take many hours per file, four times over. This needs a decision from the user/reviewing session on how to proceed (e.g., shrinking the box back toward the paper's literal 10x10x10 um^3 sensitive volume for the proton case specifically, re-enabling the mesoscopic stage despite its Species.Txt truncation trade-off, or accepting drastically fewer events/longer wall-clock runs) before this ticket's files can be verified end-to-end. Not resolved by this ticket alone — flagging to the user rather than guessing.

**Blocked (2026-10-09T18:43:12.621Z):** 90 MeV proton events take 40-70+ min each under full IRT (no meso hand-over) at the default 500 um box -- 20-event runs are infeasible; needs a design decision

**Resolved:** asked the user; they chose to shrink the box for the proton macros specifically, matching the paper's own 10x10x10 um^3 sensitive volume rather than keep the project's 500 um default. Added `/chem/env/halfBox 5 um` to all 4 macros (before `/run/initialize`), with a header-comment explanation of why (computational tractability: ~570 keV deposited over the default 1 mm box vs ~6 keV over 10 um) and its trade-off (rare long-range secondaries truncated at the smaller box edge, same caveat the paper itself accepts). Re-verified all 4 macros directly in this session (not re-dispatched):
- `beam_farokhi_proton_o2_0pct.in`: exit 0, 0 `EEEE`, 12.679 s for 20 events.
- `beam_farokhi_proton_o2_3pct.in`: exit 0, 0 `EEEE`, 10.988 s.
- `beam_farokhi_proton_o2_7pct.in`: exit 0, 0 `EEEE`, 14.526 s.
- `beam_farokhi_proton_o2_21pct.in`: exit 0, 0 `EEEE`, 15.49 s.

Each `Manifest.json` records `"chemistry": "Farokhi2023"`, `"particle": "proton"`, and the correct `scavengers` molarity (3.11e-05, 7.27e-05, 2.18e-04 M for 3/7/21pct; absent for 0pct). `Species.Txt` for the 0pct run shows 19 distinct recorded times (per ticket 2). All acceptance criteria now hold; this ticket is no longer blocked.

## Commits

- 3a1120c feat: add Farokhi2023 proton example macros (ticket 05)

## Time Spent

1h 49m (ticket-start.js to ticket-complete.js)
