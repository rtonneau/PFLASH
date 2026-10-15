# Farokhi2023 G-value comparison

Run: `python compare_farokhi2023.py` (no arguments; the run directories are a small
hardcoded list in the script, following the precedent of
`../../2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/compare_gvalues.py`).
It reads the already-produced `Species_nt_species.csv` from tickets 5 and 6 of this session
(90 MeV proton / 500 keV electron, Farokhi2023 chemistry, 0/3/7/21 % and 0/2.5/5/21 % O2
respectively) and writes two PNGs next to itself:

- `farokhi2023_proton_gvalues.png` -- one panel per species Fig. 2 of the paper tracks (OH,
  H3O+, H, e_aq, H2, H2O2, HO2, O2-), dnachem-min's G(t) curves for all four irradiated O2
  levels, with hand-digitized reference points from Fig. 2's 21 % O2 panel (the same case
  used for the Fig. 3 TRAX-CHEM comparison) overlaid as black `x` markers.
- `farokhi2023_electron_gvalues.png` -- one panel per species Fig. 5 tracks (e_aq, O2-, H,
  HO2), dnachem-min's G(t) curves for all four irradiated O2 levels, each overlaid with
  hand-digitized reference points from the matching O2-level curve of Fig. 5's four panels
  (same colour, `x` marker).

`sumG / nEvent` from the CSV is used directly as the mean G-value per event (that is what
`ScoreSpecies::ProcessHits` accumulates into `sumG`, see `src/scoring/ScoreSpecies.cc`); this
is the mean of each event's own G-value, not a ratio of total count to total energy deposit,
so it matches the convention the paper itself and `ScoreSpecies` use.

## Result (qualitative)

Both figures show dnachem-min's curves in the same shape and ordering as the paper's own
Geant4-DNA simulation: e_aq and H deplete faster at higher O2 %, O2- and HO2 build up faster
and higher at higher O2 %, consistent across both particles. dnachem-min's curves sit visibly
above the digitized reference points at late times (around 1e-7 to 1e-6 s) for the
scavenger-dependent species (e_aq, O2-, HO2) at matched O2 levels -- in the same qualitative
direction the paper's own Fig. 3/5 TRAX-CHEM-vs-Geant4-DNA comparison shows (the two codes
already disagree by a comparable margin at late times, attributed in the paper to differences
in reaction types and dissociation probabilities between codes, Section 3.1). OH, H3O+ and H2O2
track the reference points closely throughout.

## Caveats (indicative only, not a reproduction)

- dnachem-min models a single homogeneous water box (10x10x10 um^3 for the proton runs, 500 um
  default for the electron runs), not the paper's nested sensitive-volume geometry.
- dnachem-min's `PhysicsList` uses `G4EmDNAPhysics_option2`; the paper does not state which
  `G4EmDNAPhysics` option (if any beyond "Geant4-DNA") it used, so the physics settings are not
  confirmed to match exactly.
- The reference points were read by eye off pages of
  `docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf`
  rendered with PyMuPDF (`fitz`, since this machine has no `poppler`), at a handful of time
  points per curve -- approximate, hand-digitized values, not extracted with a digitizing tool.
  They carry a visible reading error (of order 0.1-0.2 G-value units) on top of whatever the
  paper's own figures already carry.

This is the same style of caveat already used for the Tonneau2025 check in
`../../2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/README.md` (there: 10 keV
electrons vs. the paper's 1.17 keV/um LET column; here: geometry, physics-option match and
hand-digitized references).
