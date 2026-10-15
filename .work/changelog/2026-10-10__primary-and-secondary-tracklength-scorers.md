---
bump: minor
floor: minor
---
- Each dump now writes `TrackLengths.Txt` / `TrackLengths.csv` with one row per event: the primary's track length, its start and end kinetic energy, how it ended (`stopped`, `escaped` or `killed`), and the summed length and count of secondary electrons (first generation and all generations).
- `Manifest.json` has a new `trackLengths` summary: event count, end-reason counts, and the mean and SEM of each column.
