# Guide to the 2026-11-17 internal deck

Deck: `20261117_PRES_PFASB_internal.pptx` (27 slides), kept outside the repository with the
other internal-meeting decks. Figures sit next to it in `ressources/`, one folder per figure
id. Each folder holds the frames, an SVG of the last frame and a `<id>.manifest.yaml`, made by
the `ppt_figures` skill (`pptfig`). Build figures are inserted as one duplicated slide per
frame, or as stacked pictures revealed in turn.

## Slide map

| Slides | Topic | Figures (`ressources/`) | Data / record |
|---|---|---|---|
| 1–3 | Title, outline, what Geant4-DNA simulates | — | — |
| 4 | Novelty 1: chemistry followed to 1 s (mesoscopic stage) | — | ADR 0006 |
| 5–6 | How the chemistry runs: IRT_syn (1 ps–5 ns), Gillespie (5 ns–1 s) | `algo_irt_syn`, `algo_gillespie` | `docs/irt-syn-mesoscopic-validation.md` |
| 7 | Novelty 2: named Chemistries | — | ADR 0002 |
| 8 | Novelty 3: from one track to a dose rate (method) | — | [simulations.md](simulations.md), ADR 0011 |
| 9 | G(t) while 1 Gy is delivered | `dr_gt_eaq`, `dr_gt_h2o2` | `docs/dose-rate/dose-rate-study.md` |
| 10 | Why G(H₂O₂) at 0.1 Gy/s goes up, then down | `dr_h2o2_budget`, `dr_h2o2_slices` | same |
| 11 | What is left when the beam stops | `dr_beamoff_radicals`, `dr_beamoff_products` | same |
| 12–13 | Movies: the box track after track (10⁴, 50 Gy/s); intermediate rates (100, 500, 10³ Gy/s) | `dose_rate_movies/movie_*Gy_s_gt.gif` | 2026-11-15 snapshot runs |
| 14 | The G of one track (sawtooth) | `pt_sawtooth` | [methods.md](methods.md) §1 |
| 15–16 | e⁻aq/·OH and H₂O₂/H₂ of one track, per rate | `pt_gtrack_eaq`, `pt_gtrack_oh`, `pt_gtrack_h2o2`, `pt_gtrack_h2` | [findings.md](findings.md) §2 |
| 17 | Pulse vs pulse: is track 500 like track 1? | `pt_dosebin_eaq_1e4`, `pt_dosebin_oh_1e4` | findings §3 |
| 18 | Who destroys the radicals of a track? | `rx_eaq_other`, `rx_oh_other` (last frame only, all four rates) | findings §4 |
| 19 | Fate of a track's radicals at 10⁴ Gy/s | `rx_eaq_fate_10000`, `rx_oh_fate_10000` (last frame only) | findings §4 |
| 20–23 | What we changed in Geant4 11.4.3: overview, patches ① finder, ② later tracks, ③ wall fold | — | [geant4-issues.md](geant4-issues.md) 1–3 |
| 24 | Fixed in our own code (mesh cap, stale records, O₂ hang, open counter crash) | — | geant4-issues 4–6, 8 |
| 25–26 | Future analyses | — | findings "Open questions" |
| 27 | Take-home | — | — |

The figure lists come from matching the deck's embedded images (MD5) against `ressources/`.

Figures made but **not in the deck**:
- `rx_eaq_own`, `rx_oh_own`;
- `rx_*_partners_1e4`, `rx_*_other_dosebin_1e4`;
- `rx_*_fate_{0p1,50,1000}`;
- `pt_dosebin_h2o2_1e4`;
- `pt_vsdose_{eaq_10ns,eaq_100ns,oh_100ns,h2o2_100ns}`;
- `dose_rate_gbeamoff` (from the earlier deck).

The budget tables `rx_budget_H2O2.csv` and `rx_budget_H2.csv` are not shown anywhere
(findings §5).

## Text that is outdated or wrong

| Slide | Where | Current text (gist) | Status | Suggested fix |
|---|---|---|---|---|
| 15 | notes | "0.1 Gy/s: tracks arrive ~2 s apart" | **wrong** | About 16 ms apart (≈ 600 tracks in 9.6 s; 0.1 Gy in 0.6–1.2 s in the pulse runs). Each track is still effectively alone up to µs, so the conclusion stands. |
| 16 | notes | "Caveat: reactions are not counted per track, so which reaction makes the difference is inferred" | **outdated** | They now are. Cite the per-track H₂O₂/H₂ budgets (findings §5): H₂O₂ made in-track 0.57–0.68 at all rates; inter-track production up to 0.14 and inter-track destruction up to 0.08; inter-track H₂ 0.30 at 10³ Gy/s. |
| 26 | slide | "Why do H₂O₂ and H₂ drop at high rate? Count which reactions consume them" | **partly done** | The per-track budgets now exist (to 100 µs). Still missing: a count per reaction channel, and the uncensored 10⁴ Gy/s budget. |
| 10 | notes | "the runs did not count which reaction fired, so this is the expected mechanism" | **still valid** | The rx runs at 0.1 Gy/s stop at 0.1 Gy (≤ 1.2 s), before the H₂O₂ decrease. A 1 Gy rx run at 0.1 Gy/s is needed (findings "Open questions"). Keep the caveat. Optionally say that the logging now exists. |
| 26 | slide | "The ·OH + H₂O₂ story of slide 10 is inferred, not counted yet" | still valid | same as above |
| 24 | slide | counter crash "hypothesis: … 1 fs tolerance" | valid | Could add that 1 fs is a project setting (`kCounterPrecisionNs`), Geant4 default 1 ps (geant4-issues 8). |
| 22 | slide | patch ② shown as a fix | valid but incomplete | The patch README says "not production-ready" (geant4-issues 2). Worth a footnote. |

### Edits made on 2026-11-17

- Slide 8: added the line "Chemistry: PureWater (65 reactions), pH 7 (bulk H₃O⁺/OH⁻ at 10⁻⁷ M),
  no O₂, IRT_syn only" (the deck did not say which chemistry was used).
- Slide 19: "pH buffer" → "bulk H₃O⁺ (pH 7)" in the subtitle and the e⁻aq box; the notes now
  say the grey band is bulk H₃O⁺ at pH 7 (fixed 10⁻⁷ M), not radiolytic H₃O⁺.
- Still open: the slide 19 figure legend (`rx_*_fate_10000`, label in `fig_rx.py`) still reads
  "by the pH buffer (bulk)".

## Numbers on the slides, with their source

| Claim | Value | Source |
|---|---|---|
| Own-track loss of e⁻aq at 10⁴ Gy/s | 0.94 (10 ns), 1.57 (1 µs), 1.74 (100 µs) | findings §4 |
| e⁻aq lost to other tracks by 100 µs | 0.01 / 0.26 / 0.78 / 0.91 (0.1 / 50 / 10³ / 10⁴ Gy/s) | findings §4 |
| ·OH lost to other tracks by 100 µs | 0.004 / 0.12 / 0.55 / 0.68 | findings §4 |
| G(e⁻aq) at 100 ns by quarter of dose, 10⁴ Gy/s | 2.32, 2.03, 1.53, 1.31 | findings §3 |
| Patch ② double trial | +4.80 ± 0.27 reactions | geant4-issues 2 |
| Finder validation | 55/55 | geant4-issues 1 |
| Mesh cap | 65536 cells per side, 15.26 nm | geant4-issues 4 |
| Stale-record fix | O₂ e⁻aq vs SBS +159 % → +9 % | geant4-issues 6 |

## Ideas for improving the deck

- One slide with the H₂O₂/H₂ budget tables (findings §5). It is the missing link between the
  radical slides (18–19) and the beam-off product slide (11).
- Use `rx_eaq_other_dosebin_1e4` next to slide 17. It shows the reaction-level reason why late
  tracks keep fewer e⁻aq.
- One "Geant4 status" table (geant4-issues summary) instead of slides 20–24 for an external
  audience, flagging which items could go upstream (1, 3, 4, 9 are small, self-contained).
- State the caveats once (findings "Caveats"). The most important are censoring at 10⁴ Gy/s,
  the shared library (SE too low) and no post-beam relaxation.
