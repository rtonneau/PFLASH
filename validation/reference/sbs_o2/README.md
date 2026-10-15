# SBS reference: air-equilibrated water (O2 21 %), 10 keV e-

Reference output of the SBS build before the IRT-syn work, kept to validate later chemistry time-step models.

- Commit: `1d047c4` (SBS chemistry time-step model; source unchanged by this capture).
- Macro: `macro/validate_o2.in` (PureWater, `/chem/env/scavenger O2 21 %` = 2.73e-4 M, pH 7, seeds `12345 67890`, `/scheduler/endTime 1 us`, 10 keV e-).
- Run: `./sim validate_o2.in --threads 10 --dir <dir>` from `build/` (RelWithDebInfo), MT, 10 threads.
- N: 1500 events, 1 `/run/beamOn`, one dump.
- Wall time: 5227.3 s (`The simulation took`). Exit 0, no `EEEE`/`WWWW`.
- Files: `Species_nt_species.csv`, `Manifest.json`. The output is not bit-reproducible, even with the seed (MT).

## Relative error on G(1 us)

Method: `Species_nt_species.csv` holds, per species and time, `sumG` and `sumG2` over events, where each event contributes
G = n_molecules / (E_deposit / eV) * 100 (molecules per 100 eV). With N = `nEvent`: mean = sumG / N,
variance = (sumG2 - sumG^2 / N) / (N - 1), standard error = sqrt(variance / N), relative error = SE / mean.
The row used is the last recorded time before 1 us (t = 999.999 ns).

Sizing: trials of 8 and 60 events gave a per-event spread for e_aq of 0.0076 and 0.0143 on G of 0.005 and 0.0138.
Almost all e_aq are scavenged by O2 before 1 us (about one molecule per event), so e_aq sets N:
the 60-event trial implied N of about 1200 for 3 %, and 1500 was run. OH, H2O2 and H2 would need under 25 events.

| Species (name in CSV) | G(1 us) [/100 eV] | per-event sd | SE | relative error |
|---|---|---|---|---|
| e_aq (`e_aq^-1`) | 0.0131 | 0.0115 | 0.00030 | 2.26 % |
| OH (`°OH^0`) | 1.9988 | 0.2417 | 0.0062 | 0.31 % |
| H2O2 (`H2O2^0`) | 0.5387 | 0.0672 | 0.0017 | 0.32 % |
| H2 (`H_2^0`) | 0.4518 | 0.0625 | 0.0016 | 0.36 % |

All four are at or below 3 %.
