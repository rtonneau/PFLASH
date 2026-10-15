# Path length / LET proxies (from existing PreChemical dumps)

These are PROXIES, not the standard physical quantities -- see the script docstring
(`track_length_let.py`) for the full derivation and caveats. Summary:

- **Path length proxy**: sum of Euclidean distances between consecutive primary-track
  (ParentID == 1) H2O interaction points logged in `PreChemical_*.txt`. Skips the
  geometric wiggle from steps that don't log a chemistry-stage species (elastic
  scattering, vibrational excitation) -- negligible bias for the proton (near-straight
  track, verified), a likely real underestimate for the electron (scattering-dominated).
- **Energy proxy**: sum of the discrete ionisation/excitation shell energies
  (`G4DNAWaterIonisationStructure`/`ExcitationStructure`) for those same interactions --
  excludes kinetic energy handed to secondary electrons, so it under-counts true dE/dx.
- **LET proxy** = energy proxy / path length proxy (keV/um). Not the NIST PSTAR/ESTAR
  stopping power.

All O2-level directories within a batch are bit-identical (same seed, O2 is
chemistry-stage only -- see `physical_stage_counts.md`); one representative O2 directory
per beam/box configuration is used here, not pooled.

## Results

| Run | Path length (um) | Energy (keV) | LET (keV/um) |
|---|---|---|---|
| 90 MeV proton, 10 um box | 4.835 +/- 0.02262 (n=16) | 0.8982 +/- 0.06433 (n=16) | 0.1855 +/- 0.01308 (n=16) |
| 90 MeV proton, 20 um box | 9.826 +/- 0.02251 (n=24) | 1.718 +/- 0.07086 (n=24) | 0.1747 +/- 0.007077 (n=24) |
| 500 keV electron | 31.09 +/- 0.5818 (n=300) | 1.713 +/- 0.02946 (n=300) | 0.06059 +/- 0.002017 (n=300) |

## Instrumented track length (TrackLengths.*)

The track length is now also scored directly: `TrackLengths.Txt`/`TrackLengths.csv` (see `docs/output/README.md`) hold, per event, the primary track's true path length (`primaryLength_nm`, from `G4Track::GetTrackLength()`) and its kinetic energy at start and end (`primaryEkin0_keV`, `primaryEkinEnd_keV`). The path length proxy above can be checked against that column. The instrumented length includes the elastic and vibrational wiggle that the proxy skips, so it is the longer of the two for the electron. Kinetic-energy loss over that length, `(primaryEkin0_keV - primaryEkinEnd_keV) / primaryLength_nm`, is a stopping-power style LET that includes the energy handed to secondary electrons, which the energy proxy above excludes. The numbers in the table above were computed from the PreChemical dumps and are not recomputed by this change.
