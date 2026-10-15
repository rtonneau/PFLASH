# Track-length statistics vs. PrimaryKiller eLossMin (500 keV electron, Farokhi2023)

Mean +/- SEM over 250 events per point, from each run's Manifest.json `trackLengths` block. See the script docstring for caveats (eLossMin sweep is a truncation-threshold check, not a dose-effect result).

| O2 | eLossMin (%) | Primary length (um) | Secondary length, all gen. (um) | Secondary count, all gen. | Secondary count, 1st gen. |
|---|---|---|---|---|---|
| 0 % | 1.0 | 31.5350 +/- 0.6435 | 6.69 +/- 1.96 | 365.9 +/- 38.4 | 92.7 +/- 1.8 |
| 0 % | 2.5 | 70.7623 +/- 1.5204 | 12.68 +/- 2.00 | 772.5 +/- 39.6 | 208.4 +/- 4.3 |
| 0 % | 5.0 | 132.9221 +/- 2.5397 | 24.78 +/- 2.81 | 1477.5 +/- 56.5 | 393.5 +/- 7.5 |
| 0 % | 7.5 | 196.0192 +/- 3.4914 | 37.34 +/- 3.70 | 2170.6 +/- 69.5 | 581.9 +/- 10.3 |
| 0 % | 10.0 | 257.6304 +/- 4.3242 | 48.32 +/- 3.72 | 2857.5 +/- 67.9 | 765.5 +/- 12.7 |
| 2.5 % | 1.0 | 31.5350 +/- 0.6435 | 6.69 +/- 1.96 | 365.9 +/- 38.4 | 92.7 +/- 1.8 |
| 2.5 % | 2.5 | 70.7623 +/- 1.5204 | 12.68 +/- 2.00 | 772.5 +/- 39.6 | 208.4 +/- 4.3 |
| 2.5 % | 5.0 | 132.9221 +/- 2.5397 | 24.78 +/- 2.81 | 1477.5 +/- 56.5 | 393.5 +/- 7.5 |
| 2.5 % | 7.5 | 196.0192 +/- 3.4914 | 37.34 +/- 3.70 | 2170.6 +/- 69.5 | 581.9 +/- 10.3 |
| 2.5 % | 10.0 | 257.6304 +/- 4.3242 | 48.32 +/- 3.72 | 2857.5 +/- 67.9 | 765.5 +/- 12.7 |
| 5 % | 1.0 | 31.5350 +/- 0.6435 | 6.69 +/- 1.96 | 365.9 +/- 38.4 | 92.7 +/- 1.8 |
| 5 % | 2.5 | 70.7623 +/- 1.5204 | 12.68 +/- 2.00 | 772.5 +/- 39.6 | 208.4 +/- 4.3 |
| 5 % | 5.0 | 132.9221 +/- 2.5397 | 24.78 +/- 2.81 | 1477.5 +/- 56.5 | 393.5 +/- 7.5 |
| 5 % | 7.5 | 196.0192 +/- 3.4914 | 37.34 +/- 3.70 | 2170.6 +/- 69.5 | 581.9 +/- 10.3 |

All O2 rows at a given eLossMin are bit-identical -- expected, see the script docstring (O2 only acts in the chemistry stage, downstream of these physical-stage track-length quantities).

