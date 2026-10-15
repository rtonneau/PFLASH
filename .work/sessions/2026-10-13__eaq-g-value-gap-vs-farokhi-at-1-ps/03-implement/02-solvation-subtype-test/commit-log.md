# Ticket 02: solvation-subtype-test

**Status:** ✅ Done

## Local Test Result

`grep -n "SolvationSubType" macro/beam_farokhi_electron_o2_0pct_meesungnoen.in` gives `10:/process/dna/e-SolvationSubType Meesungnoen2002`.

Run (done by the session after the subagent's serial 1 us run was too slow): scratch copy of the macro with `/scheduler/endTime 1.01 ns`, `build/sim.exe --threads 8`, 250 events, 540 s wall. e_aq G (mean sumG/n, Bessel SEM): 1 ps 4.150 +/- 0.006; 10 ps 4.131; 100 ps 4.044; 1 ns 3.751 +/- 0.008. Ritchie1994 at 1 ps: 4.036; stock example (Meesungnoen2002): 4.176 +/- 0.006 at 1 ps, 3.76 at 1 ns.

## Review Notes

Subagent haiku/low was BLOCKED: it ran the 250 events serially to 1 us (about 1.5 min per event). I rewrote the macro from `beam_farokhi_electron_o2_0pct_elossmin_2p5pct.in` (eLossMin 12.5 keV, the threshold the 4.036 reference and the stock run use) with only the solvation subtype changed, then ran the 1 ns variant myself.
Finding: switching Ritchie1994 to Meesungnoen2002 raises e_aq at 1 ps from 4.036 to 4.150, closing 0.114 of the 0.140 gap to the stock example (4.176). The remaining 0.026 (0.6 %, about 3 SEM) is for the dissociation-channel test (ticket 03/04). The curve at 1 ns (3.75) also matches the stock run (3.76).

## Blockers / Challenges

The macro keeps `/scheduler/endTime 1 us` like its siblings; the 1 ns scratch copy is only for this test. `build/sim.exe` needs `E:\miniconda3\envs\qt5\Libraryin` on PATH. A stale subagent kept resending its BLOCKED report and was stopped.

## Commits

- fd60b11 test: add Meesungnoen2002 variant of Farokhi 0% O2 electron macro (ticket 02)

## Time Spent

11m (ticket-start.js to ticket-complete.js)
