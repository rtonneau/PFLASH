# Instrumented track length vs. energy loss (90 MeV proton)

Real TrackLengths.csv data (not the PreChemical-dump proxy in track_length_let.md), 50 events/box, o2_0pct representative (bit-identical across O2 at the physical stage).

| Box | Energy loss, mean +/- SEM (median) (keV) | Primary length (um) | Instrumented LET from mean (keV/um) | Secondary length, all gen. (um) | Secondary count, all gen. |
|---|---|---|---|---|---|
| 20 um | 9.497 +/- 1.346 (6.491) (n=50) | 10.0000 +/- 0.0000 | 0.9497 +/- 0.1346 | 6.96 +/- 1.40 | 430.1 +/- 51.9 |
| 40 um | 21.527 +/- 2.475 (14.667) (n=50) | 20.0000 +/- 0.0000 | 1.0764 +/- 0.1237 | 14.28 +/- 1.79 | 901.4 +/- 75.3 |

## Compare against the path-length/LET proxy (track_length_let.md) and NIST PSTAR

The proxy (10 um box, PreChemical dumps, 16 events) gave 0.1855 +/- 0.01308 keV/um; the proxy (20 um box, 24 events) gave 0.1747 +/- 0.007077 keV/um -- both close to the ~0.57 keV/um NIST-PSTAR-based figure already cited in the proton macros' own header comments. The instrumented LET above (0.95, 1.08 keV/um) is higher than both by roughly 1.7-1.9x relative to the ~0.57 keV/um reference, not just 'somewhat' higher as the proxy's exclusion of secondary-electron kinetic energy alone would predict.

The per-event relative spread is large (sample std dev is close to the mean itself at both box sizes -- e.g. 20 um: std dev ~9.5 keV on a 9.5 keV mean), i.e. a few high-energy-loss events are pulling the mean well above the typical/modal value -- consistent with occasional larger-angle, larger-energy-transfer collisions (delta-ray straggling) rather than a uniform per-event loss. At n=50/box this is not yet distinguishing a real effect from a mean dominated by a handful of outlier events; a larger n and/or reporting the median alongside the mean would clarify which is going on before drawing a physics conclusion from the absolute LET value.
