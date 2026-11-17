# Per-track reaction log in the dose-rate-study `history` (2026-11-16)

Branch `feat/per-track-reactions` (uncommitted). Requested by the user: rerun the pulse-resolved
histories with reactions counted, and counted per track.

## Code (examples/dose-rate-study)
- `ReactionLog` (Geant4-free, `include/ReactionLog.hh`, `src/ReactionLog.cc`, test
  `test/ReactionLogTest.cc`): every IRT pair reaction with the source (track) of each reactant.
  Injected molecules: by track-ID range of their source. Molecules made later: reaction products
  take the later reactant's source (mixed flag when the reactants differ); dissociation and
  bulk-reaction secondaries take their G4Track parent's source.
- `ActivationObserver`: configures the ranges, learns live tracks at each activation, records every
  reaction in `UserReactionAction`.
- `HistoryMain`: writes `reactions.csv`, ledger `reactions` block, HISTORY_DONE counts.
- First-order bulk reactions (G4DNAScavengerProcess) never reach UserReactionAction: `ObserveStep` after each
  time step logs every molecule that vanished without a pair reaction as a `bulk` reaction (source -2) and
  gives parentless products the source of the nearest vanished molecule. Water (H2O^0/H2O^+) is skipped
  (the 2026-11-16 runs predate that fix; `rx_analysis.py` filters those rows).

## Runs
`plan_pulse_rx` (r50/r1000/r10000 x3) and `plan_pulse_lo_rx` (r0.1 x3): same configs and seeds as
`plan_pulse` / `plan_pulse_lo`, under `.scratch/tests/2026-11-16__pulse-by-pulse/`.

## Analysis
`.scratch/deck-20261117/rx_analysis.py` -> `pertrack_rx.csv`, `tracks_rx.csv`; `fig_rx.py` figures
(own vs other-track loss of e-aq / OH per rate, partners at 1e4 Gy/s, by dose delivered, H2O2/H2
budgets); 1-2 slides added to the 20261117 deck after the pulse-vs-pulse slide.

## Status (2026-11-17)
All 12 runs ok, unknownOrigin=0. Water skip rebuilt, 15/15 tests pass. Deck slides 18-19 added.
