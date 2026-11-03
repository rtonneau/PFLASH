---
bump: minor
floor: minor
---
- New `/primaryKiller/energyCuts <bool>` command (default true): `false` turns off every primary-electron energy cut and keeps only the box cut.
- `/primaryKiller/minKineticE` now sets the kinetic energy at or below which the primary electron is killed (it previously did nothing).
- Macro builder: "Off" for the primary-electron energy cuts now writes `/primaryKiller/energyCuts false` instead of setting both thresholds to 10 MeV.
