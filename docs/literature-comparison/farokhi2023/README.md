# Farokhi2023 G-value comparison

Time-dependent G-values from dnachem-min's `Farokhi2023` chemistry, compared against
Farokhi et al. 2023 ("Effects of the Oxygen depletion in FLASH irradiation investigated
through Geant4-DNA toolkit", Radiat. Phys. Chem. 212, 111184), with a statistical-adequacy
check on the underlying Monte Carlo runs.

## Data

Production runs (not the smaller smoke runs used during development), 8 threads, IRT_syn
particle-based stage (no mesoscopic hand-over), `/scheduler/endTime 1 us`, 61 log-spaced
recording times (10 per decade, 1 ps to ~1 us):

| Particle | Energy | Box | O2 levels | Events/run |
|---|---|---|---|---|
| proton | 90 MeV | 10x10x10 um^3 (`/chem/env/halfBox 5 um`) | 0, 3, 7, 21 % (0, 31.1, 72.7, 218 uM) | 16 |
| electron | 500 keV | 500 um default + `PrimaryKiller eLossMin 5 keV` | 0, 2.5, 5, 21 % (0, 26.0, 51.9, 218 uM) | 56 |

Run on 2026-10-09, output at `<G4_DATA_OUTPUT>/261009_Farokhi2023_{proton,electron}/o2_*`
(machine-specific directory, not part of this repo; `plot_gvalues.py` reads it via the
`G4_DATA_OUTPUT` environment variable). O2 percentages are the paper's own Henry's-law
conversion at 37 C, not dnachem-min's `%` scavenger shortcut (see the macros' header comments
for the conversion).

A follow-up box-size check for the proton run, same beam and O2 levels but a 20x20x20 um^3
box (`/chem/env/halfBox 10 um`) and 24 events each, run on 2026-10-10 at
`<G4_DATA_OUTPUT>/261010_Farokhi2023_proton_20um/o2_*`. See
`farokhi2023_proton_boxsize_gvalues.png` below.

## Regenerating

```
python plot_gvalues.py
```

Reads `Species_nt_species.csv` directly (no re-running of the simulation). Writes 6 files
next to itself: `farokhi2023_{proton,electron}_gvalues.png` (G(t) curves, shaded = +-1 SEM,
overlaid with hand-digitized reference points from the paper's Fig. 2/Fig. 5),
`farokhi2023_proton_boxsize_gvalues.png` (10x10x10 um^3 solid vs. 20x20x20 um^3 dashed, same
species panels, no reference points -- a box-size sensitivity check, not a literature
comparison), `farokhi2023_{proton,electron}_relerr.png` (relative SEM vs. time -- the
statistics check), and `statistics_summary.md` (the same check as a table).

## Statistics: is there enough?

Mean and standard error of the mean (SEM) come from each run's own `sumG`/`sumG2`
ntuple columns (sum and sum-of-squares of each *event's* own G-value,
`n_mol / eventEdep_eV * 100`), via the standard Bessel-corrected estimator:

```
mean = sumG / n
sample variance = (sumG2 - n * mean^2) / (n - 1)
SEM = sqrt(sample variance / n)
```

See `statistics_summary.md` for the full table (max relative SEM and fraction of the 61
recorded times above a 25 % relative-SEM threshold, per species/O2 level). Short version:

- **OH, H3Op, H2, H2O2** (the dominant, directly-produced species): relative SEM stays under
  ~16 % everywhere, for both particles and all O2 levels. These curves are statistically solid
  at the current event counts.
- **e_aq, H**: relative SEM stays under ~10 % for most of the 1 ps-1 us range, both particles.
  It grows late in time *only* where the mean itself is collapsing toward zero under strong O2
  scavenging (21 % O2, t > ~3e-7 s for the proton run in particular, where it reaches ~20-40 %)
  -- an artifact of dividing by a small mean, not a sign that the absolute counts are
  unreliable.
- **O2m, HO2** (the minor oxygen-scavenging products): this is where statistics are thin. Both
  species start at G = 0 and only begin forming once enough radiolytic species have diffused
  far enough to meet the dissolved O2 background, so most events still record exactly 0
  molecules through the early part of the window -- the SEM/mean ratio is at or near 100 % until
  the population builds up. For the **proton** run (16 events), this stays above the 25 %
  threshold for roughly half of the time points from onset to ~1 us at every non-zero O2 level
  (e.g. O2m at 21 % O2: 25/53 points over threshold). For the **electron** run (56 events), the
  same species clear the threshold earlier and more often stay below it afterward (e.g. O2m at
  21 % O2: 26/61 points over threshold, concentrated at the very earliest onset times) -- more
  than 3x the proton event count visibly tightens this, but does not eliminate it.
- At 0 % O2, O2m and HO2 are identically zero throughout (no scavenger reaction to fire),
  so no SEM is defined there (correctly reported as "n/a" rather than 0 %).

**Practical read:** the main-species curves (OH, H3Op, H, e_aq, H2, H2O2) are trustworthy for
shape/level comparison against the paper at these event counts. The O2m/HO2 curves' *overall
trend* (ordering by O2 level, onset time, late-time magnitude) is almost certainly right -- it
matches the paper's own curves and the proton/electron difference goes the expected direction
-- but the *exact shape* of their early rise is not well constrained at 16 events (proton) and
only partially better at 56 events (electron). Reproducing the paper's O2m/HO2 curves more
precisely would need substantially more events (order 100-300, based on how slowly the proton
relative-SEM table improves between the two event counts already run), concentrated on the
higher O2 levels where these species are most populated.

## Box-size check (10 um vs. 20 um half-box)

`farokhi2023_proton_boxsize_gvalues.png` overlays the 10x10x10 um^3 proton run (16 events,
solid) against the 20x20x20 um^3 run (24 events, dashed) at the same 4 O2 levels. For every
species and O2 level except one, the two box sizes agree within their SEM bands across the
full 1 ps - 1 us window -- the 10 um box is not visibly truncating the diffusion volume for
this beam/O2 combination. The one exception is **H at 0 % O2**: the two curves separate after
~1 ns (10 um plateaus near 0.87, 20 um near 0.78), a gap larger than either curve's SEM band.
Given the otherwise clean agreement and the modest event counts (16/24) on both sides, this is
plausibly a statistical fluctuation in one or both runs rather than a real box-size effect, but
it has not been checked with a larger run.

## Physical/pre-chemical stage interaction counts

`physical_stage_counts.py` compares, per event, how many water-ionisation, water-excitation,
dissociative-electron-attachment and secondary-electron (solvation) events each beam produces.
Source: `PreChemical_run<R>_event<E>.txt`, the per-event dump Geant4-DNA's own
`G4DNAChemistryManager`/`G4PhysChemIO` write directly from the physics models at the
physical/pre-chemical boundary -- the files `/run/dumpDataAndReset(ToDir)` moves into the dump
directory (same files referenced in `CLAUDE.md`'s `PreChemicalFiles.cc` entry). Writes
`physical_stage_counts.png` (grouped bar chart) and `physical_stage_counts.md` (full table) next
to itself.

**Caveat specific to this check:** all 4 O2-level macros in a batch share the fixed RNG seed
(`sim.cc`'s `kDefaultSeed = 12345`), and O2 only acts in the chemistry stage, downstream of these
files -- so the 4 O2 levels of a batch are *bit-identical* at the physical stage (verified by
diff), not 4 independent samples. The script uses one representative O2 directory per beam (16
proton events, 300 electron events from the newer batch, whose seed-identical prefix supersedes
the original 56-event batch) rather than pooling, and reports the per-O2-level breakdown only as
a determinism check.

Result: the 500 keV electron beam produces ~1.7x more ionisations, secondary electrons and
dissociative-attachment events per event than the 90 MeV proton beam, and ~1.55x more excitations
(ionisation/excitation ratio 7.5 for the proton vs. 8.3 for the electron). The ionisation and
secondary-electron ratios (1.70, 1.70) track the per-event energy-deposit ratio almost exactly:
4072 eV/event (proton, 10x10x10 um^3 box) vs. 6922 eV/event (electron, `PrimaryKiller eLossMin`
truncation at 1 % of the 500 keV beam energy) -- a ratio of 1.70 -- consistent with ionisation
count scaling with deposited energy. The slightly higher ionisation/excitation ratio for the
electron is consistent with its lower LET in this geometry, which favours ionisation over
excitation somewhat more than the proton's denser track.

## Path length / LET proxies

`track_length_let.py` estimates, from the same `PreChemical_run<R>_event<E>.txt` files, a path
length, an energy, and an LET (linear energy transfer) **proxy** for the primary track of each
beam -- computed entirely from data already dumped, no new simulation code or reruns. Path
length is the sum of Euclidean distances between consecutive primary-track (`ParentID == 1`)
water-ionisation/excitation interaction points; energy is the sum of their discrete
ionisation/excitation shell energies; LET = energy / path length. Writes
`track_length_let_boxsize.png` (10 vs. 20 um proton), `track_length_let_proton_vs_electron.png`
and `track_length_let.md` (full table and caveats) next to itself.

**These are proxies, not the standard physical quantities** -- see the script docstring for the
full derivation. In short: path length misses the geometric wiggle from steps that don't log a
chemistry-stage species (elastic scattering, vibrational excitation), which is a negligible bias
for the proton (near-straight track) but a likely real underestimate for the scattering-dominated
electron; the energy sum excludes kinetic energy handed to secondary electrons, so it under-counts
true dE/dx and should not be compared directly to NIST PSTAR/ESTAR stopping powers. A true
instrumented LET (per-step kinetic-energy loss and step length of the primary, via new C++
scoring code and reruns) remains available as future work.

**Result:** the proton path length matches the box geometry closely (4.84 um in the 10 um box,
9.83 um in the 20 um box -- the beam starts at the box center per `Manifest.json` and travels to
the +z face, so these are almost exactly the expected half-box chord lengths), and the **LET
proxy agrees within SEM across the two box sizes** (0.186 +/- 0.013 keV/um at 10 um vs. 0.175 +/-
0.007 keV/um at 20 um) -- the intended validation that LET, a per-unit-length quantity, should not
depend on box size, passes. Comparing beams, the 90 MeV proton's LET proxy (0.186 keV/um) is
about 3x the 500 keV electron's (0.061 keV/um), the expected qualitative ordering.

## Box-size check, re-run with instrumented track lengths (10/20/40 um box edge)

`farokhi2023_proton_boxsize_gvalues.png` now overlays three box sizes: 10x10x10 um^3
(16 events, solid, unchanged from the original run), 20x20x20 um^3 (50 events, dashed,
re-run at `261010_Farokhi2023_proton_20um_tracklen/` after the TrackLengths scorer
landed, superseding the earlier 24-event pre-tracklength run) and 40x40x40 um^3 (50
events, dotted, new). All three agree within SEM across the full 1 ps-1 us window for
every species and O2 level -- the diffusion boundary is not visibly truncating the
result even at 40 um.

`track_length_vs_energy_loss.py` uses the real per-event `TrackLengths.csv` scorer
output (not the path-length/LET proxy below) to plot primary energy loss against
primary length, secondary (all-generation) track length and secondary electron count,
one point per event, 20 um vs. 40 um box. Primary length is essentially fixed by box
geometry (10.0000 um / 20.0000 um, negligible spread); the genuinely free per-event
variable is energy loss, and both secondary track length and secondary count track it
closely and roughly linearly. The energy-loss distribution is right-skewed (sample std
dev is close to the mean at both box sizes, e.g. 20 um: ~9.5 keV std dev on a 9.5 keV
mean) -- a handful of high-energy-loss events pull the mean well above the median (20 um:
mean 9.497 keV vs. median 6.491 keV), consistent with occasional larger-angle delta-ray
collisions. The median-based LET (20 um: 0.649 keV/um; 40 um: 0.733 keV/um) sits much
closer to the ~0.57 keV/um NIST-PSTAR-based figure already used in the proton macros'
header comments than the mean-based LET does (0.95, 1.08 keV/um) -- see
`track_length_vs_energy_loss.md` for the full table and caveats.

## PrimaryKiller eLossMin sweep (500 keV electron, 0/2.5/5 % O2)

`track_length_vs_elossmin.py` sweeps the configured `/primaryKiller/eLossMin` threshold
(1, 2.5, 5, 7.5, 10 % of the 500 keV beam energy) at 3 O2 levels, reading each run's
`trackLengths` summary straight from `Manifest.json` (250 events/point). Primary length,
secondary length and secondary count all scale with eLossMin, essentially linearly over
this range (e.g. primary length: 31.5 um at 1 % to 257.6 um at 10 %). As expected, the 3
O2 curves are bit-identical in every panel (O2 only acts in the chemistry stage,
downstream of these physical-stage quantities) -- see `track_length_vs_elossmin.md` for
the full table. The 5 %/10 % point (`o2_5pct/elossmin_10pct`) was still running when this
was last generated; rerun the script once it finishes to pick it up.

## Caveats (indicative only, not a reproduction)

- dnachem-min models a single homogeneous water box (10x10x10 um^3 for the proton runs, 500 um
  default for the electron runs), not the paper's nested sensitive-volume geometry.
- dnachem-min's `PhysicsList` uses `G4EmDNAPhysics_option2`; the paper does not state which
  `G4EmDNAPhysics` option (if any beyond "Geant4-DNA") it used.
- The reference points were read by eye off rendered pages of
  `docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf`,
  at a handful of time points per curve -- approximate, hand-digitized values, not extracted
  with a digitizing tool, carrying a visible reading error (order 0.1-0.2 G-value units) on top
  of whatever the paper's own figures already carry.

This directory supersedes the earlier session-local indicative check for Farokhi2023
(`.work/sessions/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/analysis/`),
which used smaller smoke-test runs (20/50 events, 3-4 points/decade) without SEM or a
statistics check.
