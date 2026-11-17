# Main findings

Units: G in molecules / 100 eV. "Per track" means per 100 eV deposited by that track. Setup and
runs: [simulations.md](simulations.md). Estimators and their limits: [methods.md](methods.md).
Rates: 0.1, 50, 10³, 10⁴ Gy/s (3 realisations each, about 600 tracks per run, 50 to 70 at
0.1 Gy/s). Standard errors are small next to the differences quoted unless stated, but see
caveat 4.

## 1. Box G-values at beam-off (2026-11-15 study, 1 Gy)

From `docs/dose-rate/dose-rate-study.md` (10 realisations, 6 at 0.1 Gy/s):

| Species | 0.1 Gy/s | 50 Gy/s | 10³ Gy/s | 10⁴ Gy/s |
|---|---|---|---|---|
| e⁻aq | 0.007 | 0.051 | 0.355 | 1.48 |
| ·OH | 0.017 | 0.318 | 1.02 | 2.03 |
| H₂O₂ | 0.61 | 0.77 | 0.60 | 0.54 |
| H₂ | 0.74 | 1.08 | 1.00 | 0.69 |

- Radicals at beam-off rise 100–200× from 0.1 to 10⁴ Gy/s, mostly because the beam is shorter:
  many tracks are still young at beam-off. These numbers mix the dose-rate effect with the
  chemistry time (beam-off 9.6 s vs 98 µs).
- From 50 to 10⁴ Gy/s, H₂O₂ falls from 0.77 to 0.54 and H₂ from 1.08 to 0.69.
- At 0.1 Gy/s, G(H₂O₂) peaks at about 1.0 near 1 s and then decreases to 0.61 at 9.6 s (H₂
  similarly, 1.2 → 0.74). Which reactions consume them over seconds was not analysed. The chain
  ·OH + H₂ → H, then H + H₂O₂ → ·OH is a hypothesis. The reaction log now makes this testable
  (see "Open questions").

This motivated looking at single tracks: the box value cannot separate "one track's own
chemistry" from "interaction between tracks".

## 2. One track's own chemistry does not depend on the rate up to about 1 µs

Per-track G at age 100 ns (net change since the track's 1 ps state plus the pooled 1 ps yield,
tracks not yet joined by the next one; `pertrack.csv`, `fig_pertrack.py`, see
[methods.md](methods.md)):

| Species | 0.1 Gy/s | 10⁴ Gy/s |
|---|---|---|
| e⁻aq | 2.9 | 1.8 |
| ·OH | 2.9 | 2.2 |
| H₂O₂ | ≈ 0.5 | ≈ 0.5 |
| H₂ | 0.43 | 0.64 |

- H₂O₂ made by a track is the same at every rate.
- The radical deficit at 10⁴ Gy/s is not a change of the spur itself. It comes from the new
  track's radicals reacting with the radicals left by earlier tracks (section 4): the net count
  includes what the track destroyed in the background.
- The own-track reaction counts (section 4) are the same at all rates: own-track loss of e⁻aq is
  1.55–1.76 per track at 100 µs at every rate.

## 3. Pulse vs pulse at 10⁴ Gy/s: late tracks keep fewer radicals

Per-track G at 100 ns, grouped by the dose already delivered when the track arrives (quarters of
the 1 Gy):

| Dose already delivered | 0–0.25 Gy | 0.25–0.5 Gy | 0.5–0.75 Gy | 0.75–1 Gy |
|---|---|---|---|---|
| G(e⁻aq) | 2.32 | 2.03 | 1.53 | 1.31 |
| G(·OH) | 2.56 | 2.43 | 2.04 | 1.94 |
| G(H₂O₂) | 0.51–0.54 | | | |

As the background of radicals left by earlier tracks builds up, each new track loses more e⁻aq
and ·OH to it. H₂O₂ made by a track does not change.

## 4. Who destroys a track's molecules (reaction log)

Cumulative count of a track's molecules destroyed, by channel (`pertrack_rx.csv`,
`rx_analysis.py` / `fig_rx.py`).

**Own track:**
- e⁻aq: 0.94 by 10 ns, 1.57 by 1 µs, 1.74 by 100 µs; 1.55–1.76 at every rate.
- ·OH: 2.59 by 1 µs, 2.75 by 100 µs.

**Other tracks, by 100 µs:**

| Rate | e⁻aq | ·OH |
|---|---|---|
| 0.1 Gy/s | 0.01 | 0.004 |
| 50 Gy/s | 0.26 | 0.12 |
| 10³ Gy/s | 0.78 | 0.55 |
| 10⁴ Gy/s | 0.91 | 0.68 |

**Bulk (pH 7 buffer, e⁻aq + H₃O⁺(B)):**
- e⁻aq: 0.46 at 0.1 Gy/s, 0.26 at 10⁴ Gy/s. At high rate the other tracks take e⁻aq before
  the buffer does.
- ·OH has no bulk loss in this chemistry.

**Partners from other tracks at 10⁴ Gy/s (by 100 µs):**
- e⁻aq is destroyed by H₃O⁺ (0.34), e⁻aq (0.19) and H (0.17).
- ·OH is destroyed by e⁻aq (0.73) and ·OH (0.32).

The H₃O⁺ partner is the radiolytic H₃O⁺ of earlier tracks, which builds up at high rate
(G(H₃O⁺) at beam-off 1.42 at 10⁴ Gy/s vs 0.007 at 0.1 Gy/s). This is not the bulk buffer.

**Dose dependence at 10⁴ Gy/s:** the loss to other tracks grows with the dose already delivered
(figure `rx_eaq_other_dosebin_1e4`). This is the reaction-level counterpart of section 3.

**Conclusion:** the dose-rate effect lives in the **1–100 µs** window. Before 1 µs a track
reacts with itself as if alone. After the spur has expanded (about 1 µs), its surviving radicals
meet either the buffer (low rate) or other tracks' molecules (high rate).

## 5. H₂O₂ and H₂ budgets per track (at 100 µs)

**H₂O₂:**

| Rate | Made, own track | Made with other tracks | Destroyed, own track | Destroyed by other tracks |
|---|---|---|---|---|
| 0.1 Gy/s | 0.633 | 0.003 | 0.131 | 0 |
| 50 Gy/s | 0.674 | 0.060 | 0.146 | 0.002 |
| 10³ Gy/s | 0.679 | 0.139 | 0.146 | 0.045 |
| 10⁴ Gy/s | 0.572 | 0.010 | 0.120 | 0.080 |

**H₂:**

| Rate | Made, own track | Made with other tracks | Destroyed, own track | Destroyed by other tracks |
|---|---|---|---|---|
| 0.1 Gy/s | 0.244 | 0 | 0.004 | 0 |
| 50 Gy/s | 0.269 | 0.077 | 0.003 | 0.0001 |
| 10³ Gy/s | 0.284 | 0.295 | 0.001 | 0.0002 |
| 10⁴ Gy/s | 0.221 | 0.030 | 0 | 0 |

No H₂O₂ or H₂ is destroyed by bulk reactions in this chemistry.

- About 0.6–0.7 H₂O₂ per track is made within the track at every rate. Inter-track production
  adds up to 0.14 (10³ Gy/s), and inter-track destruction reaches 0.08 at 10⁴ Gy/s.
- Inter-track H₂ (e⁻aq + e⁻aq, e⁻aq + H, H + H of different tracks) is large at 10³ Gy/s: 0.30,
  more than the own-track 0.28. This is consistent with the box H₂ staying high at 50–10³ Gy/s (interpretation, not tested).
- **The 10⁴ Gy/s row is censored** and not comparable to the others for the "with other tracks"
  columns. Its beam lasts about 100 µs, so only the tracks that arrived early can be followed to
  100 µs. Those tracks had few neighbours yet. The 0.010 and 0.030 are therefore underestimates
  of a typical 10⁴ Gy/s track.

## Caveats

1. **Censoring.** A track can be followed only up to beam-off (and, for `species_age.csv`, up
   to the next arrival). Late ages are seen only for early tracks. The analysis keeps, at each
   age, only the tracks observed that long (`rx_tables.gcum`), which biases high-rate,
   late-age values toward early tracks.
2. **Bulk-product attribution by position.** Products of bulk reactions have no parent in
   Geant4 (issue 9) and are matched to the nearest molecule that vanished in the same time step.
   654–6923 matches per run; a wrong match would move a product between tracks, never lose it.
3. **Per-track G (sections 2–3) is a net box change** since the track's 1 ps state, plus the
   pooled initial yield, over the tracks not yet joined by the next one. It includes background
   molecules the track destroyed, and the background's own evolution (small before about 100 ns).
   It is not "molecules of this track". The reaction-log numbers (sections 4–5) are the
   attributed counts.
4. **The standard error is too low.** All runs draw from one 2400-packet library (about 600
   packets per run), so runs share packets. The SE treats them as independent.
5. **No post-beam relaxation.** The chemistry stops 1 ps after the last activation. The box
   values are not final yields.
6. **Reflecting box.** A 4 µm box with mirror walls is a periodic-like lattice of mirrored
   tracks, not an infinite medium. The dose rate sets the mean gap between tracks, but their
   spatial overlap also depends on the box size.
7. **Only 3 realisations per rate**, and 0.1 Gy/s goes to 0.1 Gy only (50–70 tracks).
8. **Model choices:** IRT_syn alone (no mesoscopic stage), pure water, no O₂, `PureWater`
   reaction set and rates, patched Geant4 (the timed-injection patch is experimental, issue 2).

## Open questions (now answerable from the data)

- Which reactions consume H₂O₂ and H₂ after 1 s at 0.1 Gy/s (section 1)? That needs a
  0.1 Gy/s, 1 Gy rx run (about 3e4 s, and above the 8.6 s crash limit of issue 8 unless
  `kCounterPrecisionNs` is raised).
- Uncensored 10⁴ Gy/s budgets: follow the tracks after beam-off (post-beam relaxation), or
  report the budget at 10 µs instead of 100 µs.
- Box-size dependence of the inter-track terms (4 µm vs larger box).
