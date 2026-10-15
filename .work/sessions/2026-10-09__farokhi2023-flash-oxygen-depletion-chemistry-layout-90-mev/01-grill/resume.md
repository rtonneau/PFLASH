# Session: Farokhi2023 FLASH oxygen-depletion chemistry layout (90 MeV proton Fig2/Fig3, 0.5 MeV electron Fig5)

**Date:** 2026-10-09T15:05:03.992Z
**Status:** Grill phase complete

## Problem Statement

dnachem-min has no chemistry layout able to reproduce Farokhi et al. 2023 ("Effects of the Oxygen depletion in FLASH irradiation investigated through Geant4-DNA toolkit", Radiat. Phys. Chem. 212, 111184), stored at `docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf`. The paper computes time-dependent G-values of water-radiolysis species at several dissolved-O2 concentrations, for a 90 MeV proton beam (Fig. 2, Fig. 3) and a 0.5 MeV electron beam (Fig. 5), using a minimal reaction set (the Geant4-DNA "scavenger" example's 9 base reactions + 3 O2-scavenger reactions) that is narrower than any Chemistry currently registered in the project (`PureWater`, `BoscoloChem`, `Tonneau2025`).

## Context & Constraints

- **Current behavior:** The project's selectable-chemistry architecture (ADR 0002, ADR 0008) already supports adding a new named Chemistry to `src/chemistry/catalog/` and registering it in `BuiltInChemistries.cc`. `PureWater`'s bulk-reaction list already contains the paper's 3 O2-scavenger reactions (`e_aq+O2`, `H+O2`, `Om+O2`) at the exact same rates (verified against `examples/extended/medical/dna/scavenger/Rtable_O2.txt` in the Geant4 11.4.1 source tree), but bundled with an acid-base buffer and an extended O2-redox network the paper does not use. `PhysicsList.cc` hardcodes `G4EmDNAPhysics` (no option suffix) for every Chemistry; the paper used `G4EmDNAPhysics_option2`. The geometry is a single homogeneous water box (`/chem/env/halfBox`, default 500 µm half-side) that is simultaneously the transport volume, the chemistry diffusion boundary and the bulk-scavenger volume — there is no nested scoring sub-volume. `ScoreSpecies.cc` records G-values at 7 fixed log-spaced times (1, 10, 100 ps; 1, 10, 100 ns; ~1 µs), one per decade, shared by every Chemistry. A dormant, undocumented `PrimaryKiller` primitive scorer (`header/scoring/PrimaryKiller.hh`, `src/scoring/PrimaryKiller.cc`) already implements track-segment truncation (kill the primary after it deposits a chosen energy) via `/primaryKiller/eLossMin` etc., registered on the detector's `mfDetector` but commented out in every existing macro and absent from CLAUDE.md.
- **Pain point:** No macro/Chemistry combination today reproduces the paper's reaction network, O2-concentration sweep, or electron track-segment method; the shared physics constructor and time-recording grid also don't match the paper's setup.
- **Dependencies:** Must not change `PureWater`, `BoscoloChem`, or `Tonneau2025` behavior; must not introduce voxelization or a second geometry concept beyond the single homogeneous box; existing CTest suite (pure-logic unit tests, no pinned physics-simulation numbers) must keep passing.
- **Tech stack:** Geant4 11.4.1 with Geant4-DNA (physics + chemistry), C++20, CMake; project's `ChemistryRegistry`/`ChemistryTypes` catalog pattern; CTest for `build-ninja/` (Debug) unit tests.

## Success Metrics

- A new `Farokhi2023` Chemistry is registered and selectable via `/chem/select Farokhi2023`, using the 9 base pure-water reactions (shared with `PureWater`) plus only the 3 O2-scavenger bulk reactions from the paper's Table 2 (`e_aq+O2→O2⁻`, `H+O2→HO2`, `O⁻+O2→O3⁻`), no acid-base buffer, no extended O2-redox network.
- `PhysicsList.cc` constructs `G4EmDNAPhysics_option2` instead of `G4EmDNAPhysics`; the previous `G4EmDNAPhysics` construction stays in the file as a commented-out line.
- `PrimaryKiller` is validated against the current chemistry pipeline, documented in CLAUDE.md, and wired into the electron macros via `/primaryKiller/eLossMin 5 keV` (1% of the 0.5 MeV beam energy).
- `ScoreSpecies.cc`'s `AddTimeToRecord` grid is extended to ~3–4 log-spaced points per decade between 1 ps and 1 µs, for every Chemistry.
- 8 new example macros exist under `macro/`: 4 for the 90 MeV proton beam (O2 = 0, 31.1, 72.7, 218 µM, matching the paper's 0/3/7/21% at 37 °C) and 4 for the 0.5 MeV electron beam (O2 = 0, 26.0, 51.9, 218 µM, matching 0/2.5/5/21%), each selecting `Farokhi2023`, disabling the mesoscopic stage, and setting the O2 scavenger concentration via explicit µM values (not the project's `%` shortcut, whose implied Henry's constant doesn't match the paper's 37 °C value).
- A session-local analysis/plotting script compares dnachem-min's `Species.Txt/csv` output against digitized reference points from the paper's Fig. 2, Fig. 3 and Fig. 5, in the same indicative-only spirit as the existing Tonneau2025 G-value check.
- `ctest` in `build-ninja/` and a full build in `build/` both still succeed; no existing macro, Chemistry, or test changes behavior other than the deliberate project-wide physics-constructor switch and time-grid extension.

## Architecture & Approach

Add the new reaction set as `src/chemistry/catalog/Farokhi2023Reactions.{hh,cc}` (portable convention, like `PureWaterReactions.cc`), reusing `PureWaterReactions::BuildPureWaterReactions()` for the shared 9 base reactions and defining a 3-entry bulk-reaction list of its own. Register it as `"Farokhi2023"` in `BuiltInChemistries.cc`, next to `PureWater`/`BoscoloChem`/`Tonneau2025`. No new molecules are needed: `O-`/`O3-` and the rest of the shared molecule table already exist (used by `PureWater`/`Tonneau2025`).

Switch `PhysicsList.cc`'s constructor from `G4EmDNAPhysics` to `G4EmDNAPhysics_option2`, leaving the old line commented out immediately above/below the new one for easy reference/revert. This is a project-wide, deliberate change (affects numeric output of every existing Chemistry/macro) — no ADR, since the grill explicitly decided this doesn't meet the bar (reversible by uncommenting, and the trade-off is already fully captured here).

For geometry: the proton macros use the project's existing default box (no `/chem/env/halfBox` override) — at 90 MeV the proton's LET (~0.57 keV/µm) is essentially constant over the 1 mm box, which fully contains the longest secondary-electron ranges, so G-values computed over the whole box approximate the paper's small nested "sensitive volume" without truncation bias and without adding a second geometry concept.

For the electron macros: validate `PrimaryKiller` works correctly with the current chemistry/scoring pipeline (it predates and was never wired into the documented architecture), then use it as designed — `/primaryKiller/eLossMin 5 keV` truncates the 0.5 MeV primary's track once it has deposited 1% of its initial energy, keeping the simulated segment's LET close to the nominal 0.5 MeV value (mirroring the paper's track-segment method, whose numeric threshold it doesn't state). Document `PrimaryKiller` in CLAUDE.md's Key Files section once validated.

Extend `ScoreSpecies.cc`'s hardcoded `AddTimeToRecord` sequence to ~3–4 log-spaced points per decade from 1 ps to 1 µs (affects every Chemistry's `Species.Txt/csv` output, not just this one).

Add 8 macros under `macro/` (`beam_farokhi_proton_*.in` × 4, `beam_farokhi_electron_*.in` × 4; exact file-naming for the O2 values finalized during ticket drafting), each: `/chem/select Farokhi2023`, `/chem/meso/enable false` (IRT_syn alone, matching the paper's IRT method and its 1 µs end time), `/chem/env/scavenger O2 <value> uM` using the paper's exact molar concentrations, appropriate `/gun/particle` + `/gun/energy`, and (electron macros only) the `/primaryKiller/eLossMin` line.

Add a session-local analysis script (Python, reading `Species.Txt/csv`) that plots dnachem-min's G-value curves against a small set of digitized reference points taken from the paper's Fig. 2/Fig. 3/Fig. 5, stored under this session's `analysis/` folder — indicative only, not shipped as project infrastructure (same precedent as the existing Tonneau2025 check).

## Assumptions & Trade-offs

- We are NOT reproducing Fig. 4 (5 MeV protons, 3-code comparison) or Fig. 6/Fig. 7 (OER-weighted dose under dynamic oxygenation, dose-rate integration) — out of scope, not requested.
- We are NOT building a true two-tier geometry (separate scoring sub-volume nested inside a larger transport box) for the proton case; the single-box approximation is expected to be at least as physically sound as the paper's setup for this purpose (no truncation bias), but is not a literal replica of their geometry.
- We are NOT pinning Henry's-law temperature/units project-wide; the `%` scavenger shortcut's existing conversion is left as-is (used by other Chemistries/macros), and the new Farokhi macros simply bypass it by specifying µM directly.
- The `G4EmDNAPhysics_option2` switch is accepted as a project-wide, hard-to-reverse-in-spirit (but mechanically trivial to revert by uncommenting) change to numeric output for every existing Chemistry and macro; no regression is expected in build or test results since no test pins physics-simulation numbers.
- The electron track-segment threshold (`eLossMin = 5 keV`, 1% of 0.5 MeV) is our own reasoned choice, since the paper does not state its numeric threshold.
- Event counts (`/run/beamOn N`) per macro are left as an implementation/ticket-drafting detail — enough for the analysis script to produce a sane trend, not necessarily publication-grade statistics; adjustable by anyone rerunning the macros.

## Open Questions

- None outstanding; all decisions above were explicitly settled with the user across four grill rounds.

## Notes

- Confirmed against primary sources during the grill: the Geant4 11.4.1 install tree's `examples/extended/medical/dna/scavenger/Rtable_O2.txt` exactly matches the paper's Table 2 (resolves OCR ambiguity in the PDF's reaction table — `[H2]` is `[O2]`, `HH2` is `HO2`).
- `PureWaterReactions::BuildPureWaterBulkReactions()` (src/chemistry/catalog/PureWaterReactions.cc:156-178) already carries the same 3 scavenger reactions at the same rates as the paper, confirming the molecule/rate data is already correct in the project; `Farokhi2023` only needs to narrow which reactions are included, not introduce new rate constants.
- `PrimaryKiller` is pre-existing, dormant code (not written during this session) — the grill decided to validate and document it rather than treat it as dead code to remove, since it directly implements the method the paper describes.
