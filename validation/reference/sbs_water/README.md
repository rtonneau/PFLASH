# SBS reference: pure water, 10 keV e-

Reference output of the SBS build before the IRT-syn work, kept to validate later chemistry time-step models.

- Commit: `1d047c4` (SBS chemistry time-step model; source unchanged by this capture).
- Macro: `macro/validate_water.in` (PureWater, seeds `12345 67890`, `/scheduler/endTime 1 us`, 10 keV e-).
- Run: `./sim validate_water.in --threads 10 --dir <dir>` from `build/` (RelWithDebInfo), MT, 10 threads.
- N: 40 events, 1 `/run/beamOn`, one dump.
- Wall time: 109.4 s (`The simulation took`). Exit 0, no `EEEE`/`WWWW`.
- Files: `Species_nt_species.csv`, `Manifest.json`. The output is not bit-reproducible, even with the seed (MT).

## Relative error on G(1 us)

Method: `Species_nt_species.csv` holds, per species and time, `sumG` and `sumG2` over events, where each event contributes
G = n_molecules / (E_deposit / eV) * 100 (molecules per 100 eV). With N = `nEvent`: mean = sumG / N,
variance = (sumG2 - sumG^2 / N) / (N - 1), standard error = sqrt(variance / N), relative error = SE / mean.
The row used is the last recorded time before 1 us (t = 999.999 ns). N was sized from a trial of 8 events
(spread per event) and checked on the final run.

| Species (name in CSV) | G(1 us) [/100 eV] | per-event sd | SE | relative error |
|---|---|---|---|---|
| e_aq (`e_aq^-1`) | 1.2445 | 0.2015 | 0.0319 | 2.56 % |
| OH (`°OH^0`) | 1.5973 | 0.2238 | 0.0354 | 2.22 % |
| H2O2 (`H2O2^0`) | 0.4293 | 0.0667 | 0.0105 | 2.46 % |
| H2 (`H_2^0`) | 0.7130 | 0.0709 | 0.0112 | 1.57 % |

All four are at or below 3 %.
