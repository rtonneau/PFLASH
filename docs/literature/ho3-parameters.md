# HO3 molecule parameters (Tonneau2025 Chemistry)

HO3 appears in Table 2 of Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, in R30 (H + O3 -> HO3), R39 (HO2 + O3 -> HO3 + O2) and R55 (HO3 -> O2 + OH). Geant4 11.4.1 has no HO3 molecule, so the `Tonneau2025` Chemistry creates it as an **extra molecule**.

## Values used

| Parameter | Value | Basis |
|---|---|---|
| Diffusion coefficient D | 2.0e-9 m2/s | **Analogue**: the value Geant4 uses for O3 (`G4O3.cc`, D = 2.0e-9 m2/s). Approved by the user on 2026-10-06. |
| vdW radius | 0.20 nm | **Analogue**: the radius Geant4 sets for O3 in `G4ChemDissociationChannels_option1.cc` (`SetVanDerVaalsRadius(0.20 * nm)`). |
| Charge | 0 | HO3 is a neutral radical. |

These are assumptions, not measurements. No literature value was found for the HO3 diffusion coefficient.

## What was searched (no HO3 diffusion coefficient found)

- Tonneau et al. 2025: HO3 only in R30, R39, R55; the paper gives no diffusion coefficients.
- Geant4 11.4.1 `source/` and `examples/` (including UHDR and chem6): no HO3 definition.
- arXiv 2601.02132 (hybrid continuum/Monte Carlo), Table S1: D listed for HO, e_aq, H2O2, H2, H, H3O+, HO- only.
- arXiv 2409.11993 and the MPEXS2.1-DNA update (PMC12075733) main text: no HO3. The MPEXS2.1-DNA supplement could not be retrieved.
- Web searches for an HO3 diffusion coefficient: nothing.

## Effect

HO3 only enters through R30, R39 and R55. Its D sets how fast it diffuses before reacting or decaying (R55, k = 1.1e5 s-1, so a lifetime of about 9 us). Replace these values if a source is found; the single place to change is the HO3 creation in `Tonneau2025Reactions.cc`.
