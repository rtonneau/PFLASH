# Methods: per-track G and the per-track reaction log

Two ways to give a G-value to one track inside a shared multi-track chemistry. They answer
different questions.

## 1. Per-track G from the box counts (`species_age.csv`)

**Data:** with `ageGridPointsPerDecade = 20`, `history` samples the box after each arrival k on
a log grid of the age τ = t − t0_k, from 1 ps up to the next arrival. Each row of
`species_age.csv` is (`arrivalIndex`, `ageNs`, `species`, `count`).

**Table** (`.scratch/deck-20261117/pulse_analysis.py` → `pertrack.csv`), one row per run,
track, age and species:

    dN_k(τ) = N(k, τ) − N(k−1, last age before arrival k)      (baseline 0 for k = 0)
    G_k(τ)  = 100 · dN_k(τ) / E_k                               (E_k = deposit of track k, eV)

`Glin` also subtracts the background drift measured just before the arrival (linear trend).

**Estimator used in the figures and in findings sections 2–3** (`fig_pertrack.py`). It
differs from the table column `G`:

    dN1_k(τ) = N(k, τ) − N(k, 1 ps)                              (change since the track's 1 ps state)
    G(τ)     = G0 + 100 · Σ_k dN1_k(τ) / Σ_k E_k                 (ratio of sums over a group)

- G0 is the initial yield at 1 ps, pooled over all tracks of all rates. It is the same at every
  rate, because all runs draw from one packet library.
- The sum runs only over the tracks **still alone at τ**: the next track has not yet arrived.
  The gaps are Poisson, so this selection does not bias the chemistry. A curve stops when fewer
  than 30 % of the tracks are still alone.
- Groups: rate, or rate and dose already delivered (`doseBefore`, 0.1 Gy bins in
  `fig_pertrack.py`; the quarters in findings section 3). The SE comes from the delta method.
- The background's own evolution during τ is attributed to the track. It is small within about
  100 ns.

**Meaning:** the net change of the box that follows track k. It includes the background
molecules the track destroyed (or helped produce). At high rate it is lower than the track's own
production. It does not say *who* destroyed what.

## 2. Per-track reaction log (`reactions.csv`)

**Code:** `examples/dose-rate-study/include/ReactionLog.hh`, `src/ReactionLog.cc` (Geant4-free,
tested by `test/ReactionLogTest.cc`), fed by `ActivationObserver`.

**Origin of every molecule** (its source = the arrival whose track made it):

- **Injected molecule:** the track ID lies in the ID range of its source (`SourceInjector`);
  `Configure()` takes those ranges.
- **Pair-reaction product:** Geant4 11.4.3 pushes the products before `G4ITModelProcessor`
  calls `UserReactionAction`, so their IDs are known there. Both reactants from one source:
  that source. From two sources: the **later** source (largest index), flagged `mixed`.
- **Dissociation product:** a `G4ITStepProcessor` secondary whose parent ID is the molecule
  it came from (`G4ITStepProcessor2.cc`, `DealWithSecondaries`); resolved through the parent.
- **Bulk-reaction product** (`G4DNAScavengerProcess`): parent ID 0, built at the reactant's
  position (Geant4 issue 9). `ObserveStep()` runs after each time step with the live
  molecules. It gives such a product the origin of the nearest molecule that vanished in that
  step without a pair reaction. An IRT_syn step carries about one reaction, so the match is
  rarely ambiguous.
- Anything else stays **unknown** (source −1). There were 0 in all runs.

**Bulk reactions:** every molecule that vanished without a pair reaction is recorded by
`ObserveStep` as a reaction with partner `bulk` (source −2), at the time of the step that ends
after it. Pre-chemical water (`H2O^0`, `H2O^+`) is excluded, because it vanishes by
dissociation (that fix came after the 2026-11-16 runs; the analysis filters those rows).

**Output:**

- `reactions.csv`: `timeNs,reactantA,sourceA,mixedA,reactantB,sourceB,mixedB,products`, one row
  per reaction.
- ledger `reactions` block: `logged`, `interSource`, `unknownOriginReactants`,
  `bulkProductsMatchedByPosition`, `bulkReactions`.
- `HISTORY_DONE` console line.

**Analysis** (`rx_analysis.py` → `pertrack_rx.csv`, `tracks_rx.csv`, `taubins_rx.csv`):

- **Loss rows:** one per reactant, seen from its own track k, at age τ = t − t0_k, with the
  partner species and a channel:
  - `own`: partner from the same track;
  - `older`: from an earlier track (the background);
  - `newer`: from a later track;
  - `unk`: unknown partner;
  - `bulk`: first-order bulk reaction.

  "Other tracks" in the findings = `older` + `newer`.
- **Gain rows:** one per product, credited to the later reactant's track:
  - `own`: both reactants from k;
  - `inter`: one reactant from another track;
  - `bulk`: made by a bulk reaction of a molecule of k.
- **Binning:** τ in log bins, 10 per decade, 1 ps to 10 s.
- **Cumulative G** (`rx_tables.gcum`): 100·Σ_k N_k(≤ τ) / Σ_k E_k over the tracks still observed
  at τ (`tauMax ≥ τ`, `tauMax` = beam-off − t0_k). SE by the ratio-estimator formula.

**Meaning:** an attributed count. "Destroyed by other tracks" is the number of a track's
molecules consumed in reactions whose partner came from another track. These are not net
changes, and they are not the same quantity as section 1.

## What neither method gives

- **Molecules made from two tracks are credited to one.** A `mixed` product goes to the later
  track. The budget tables count it under "made with other tracks" of that track only.
- **No uncensored late times at high rate.** No track is followed after beam-off.
- **Independent errors.** Runs share the 2400-packet library, so the SE is too low.
- **Spatial overlap.** Neither method separates "close in time" from "close in space". The
  reflecting box mirrors tracks near the walls.
