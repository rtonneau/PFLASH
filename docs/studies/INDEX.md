# Study index

Simulation studies of this project (`/study` skill). Data live under the machine-local
`STUDIES_ROOT` and never in git; paths resolve only on the machine named.

| Study | Question | Status | Machine | Data | Report |
|---|---|---|---|---|---|
| 2026-11-14__dose-rate-gvalue-study | Does the dose rate change the radiolysis G-values of pure water at 1 ms, for rates from conventional (0.1 Gy/s) up to 1000 Gy/s, under a continuous beam? | done | SPHY-21-19 | `${STUDIES_ROOT}/PFLASH/2026-11-14__dose-rate-gvalue-study` | [docs/dose-rate/continuous-beam-gvalues.md](../dose-rate/continuous-beam-gvalues.md) |
| 2026-11-15__dose-rate-study | How do the radiolysis G-values of pure water change with the dose rate when a 4 um water box is irradiated by a continuous 500 keV electron beam up to 1 Gy, for rates from 0.1 Gy/s to 1e4 Gy/s? | done | SPHY-21-19 | `${STUDIES_ROOT}/PFLASH/2026-11-15__dose-rate-study` | [docs/dose-rate/dose-rate-study.md](../dose-rate/dose-rate-study.md) |
| 2026-11-16__pulse-by-pulse | How does the dose-rate effect on pure-water radiolysis build up track by track (per-track G and who destroys whose molecules) for 500 keV electrons in a 4 um reflecting box at 0.1 to 1e4 Gy/s? | done | SPHY-21-19 | `${STUDIES_ROOT}/PFLASH/2026-11-16__pulse-by-pulse` | [docs/dose-rate-pulse-study/README.md](../dose-rate-pulse-study/README.md) |
