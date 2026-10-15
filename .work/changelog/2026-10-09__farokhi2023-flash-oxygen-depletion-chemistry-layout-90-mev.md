---
bump: minor
floor: minor
---
- Add a `Farokhi2023` chemistry (`/chem/select Farokhi2023`) reproducing the FLASH oxygen-depletion radiolysis network of Farokhi et al. 2023 (Radiat. Phys. Chem. 212, 111184).
- Add 8 example macros reproducing the paper's Fig. 2/Fig. 3 (90 MeV proton) and Fig. 5 (0.5 MeV electron) conditions at their O2 concentrations.
- Switch the default physics constructor from `G4EmDNAPhysics` to `G4EmDNAPhysics_option2`, affecting every Chemistry's simulated output.
- Record species G-values at a finer time grid (19 log-spaced points from 1 ps to 1 µs, up from 7).
- Validate and document the `PrimaryKiller` track-segment-truncation feature (`/primaryKiller/eLossMin` etc.), previously undocumented.
