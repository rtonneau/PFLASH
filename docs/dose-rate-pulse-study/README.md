# Dose-rate study, pulse by pulse and reaction by reaction (2026-11-16/17)

What was done in the session of 2026-11-16 to 2026-11-17. It continues the dose-rate study of
2026-11-15 (`docs/dose-rate/dose-rate-study.md`). The work produced the slides of the internal
meeting of 2026-11-17 (`20261117_PRES_PFASB_internal.pptx`, kept outside the repository) and the
simulations and analysis behind them.

Written for two readers:

- an agent that investigates or prepares a **Geant4(-DNA) fix**: start with
  [geant4-issues.md](geant4-issues.md);
- an agent that **improves the slide deck**: start with [deck-guide.md](deck-guide.md), then
  [findings.md](findings.md).

## Files

| File | Content |
|---|---|
| [geant4-issues.md](geant4-issues.md) | Every Geant4 11.4.3 issue found in this project's dose-rate work: where in the source, how it shows up, status (patched, worked around, open), and why. |
| [patch-necessity.md](patch-necessity.md) | Test suite for patches 1-3: each fixes its defect on a fixture; leave-one-out study histories at 10⁴ and 0.1 Gy/s reach only patch 2, once. |
| [simulations.md](simulations.md) | Every simulation set: aim, main parameters, seeds, cost, output location. |
| [findings.md](findings.md) | Main results with numbers, and their caveats. |
| [methods.md](methods.md) | How the per-track G and the per-track reaction log are computed, and what they cannot tell. |
| [reproduction.md](reproduction.md) | Code changes (uncommitted), scripts, commands, data files. |
| [deck-guide.md](deck-guide.md) | Slide map of the 2026-11-17 deck, figure folders, text that is now outdated or wrong. |

## In one paragraph

Up to 1 Gy of 500 keV electrons is delivered as single tracks at random (Poisson) times into a
4 µm reflecting water box, with one shared IRT_syn chemistry (patched Geant4 11.4.3, `PureWater`
chemistry, pH 7, no O₂). The rates are 0.1, 50, 10³ and 10⁴ Gy/s. The spur chemistry of a track is the
same at every rate up to about 1 µs. The dose-rate effect lives between 1 and 100 µs, when the
radicals that escaped their own track meet the radicals of other tracks instead of the pH
buffer. At 10⁴ Gy/s a late track keeps fewer e⁻aq than an early one: G(e⁻aq) at 100 ns falls
from 2.3 to 1.3 between the first and the last quarter of the 1 Gy.
The new per-track reaction log measures the mechanism directly: by 100 µs, other tracks
destroy 0.9 e⁻aq per 100 eV of a track at 10⁴ Gy/s, against 0.01 at 0.1 Gy/s.

## Status

- Code: branch `feat/per-track-reactions`, **uncommitted** (see [reproduction.md](reproduction.md)).
- Data and analysis scripts: under `.scratch/` (git-ignored, local only). Archive them before
  cleaning the scratch tree.
- No Geant4 report has been sent upstream. The draft notes are in
  `examples/poisson-track-irt/docs/upstream-notes.md`; [geant4-issues.md](geant4-issues.md)
  adds what those notes do not cover.
- The project rule "plan mode before non-trivial changes" was not applied to these docs. The
  user specified their content directly.

## Related records

- `docs/dose-rate/README.md`: index of the earlier dose-rate investigations (2026-11-06 to 2026-11-15).
- `docs/dose-rate/dose-rate-study.md`: the 1 Gy production study that this session builds on.
- `docs/adr/0010-poisson-track-irt-stock-build.md` (reverted stock build), `docs/adr/0011-reflecting-box-for-dose-rate-study.md`.
- `patches/geant4/11.4.3/`: the Geant4 patches.
- `.claude/plans/2026-11-16-per-track-reactions.md`: plan of the reaction-log change.
