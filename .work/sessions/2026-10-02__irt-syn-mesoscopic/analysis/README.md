# Analysis scripts: irt-syn-mesoscopic session (2026-10-02)

These are the scripts behind the numbers in `docs/irt-syn-mesoscopic-validation.md` and ADR 0006. The reusable dump comparison is `analysis/compare_reference.py` at the repo root; the scripts here are the session-specific ones. Run them with the conda env `GEANT4_py311` (pandas, matplotlib), using `PYTHONIOENCODING=utf-8` on Windows, which `°OH` needs. Scripts that import `compare_reference` find it from their own location, so they can be run from any directory.

| Script | Inputs | Produced |
|---|---|---|
| `reference_relative_error.py` | a `Species_nt_species.csv` | Relative standard error of G(1 µs) per species, from `sumG` / `sumG2` / `nEvent` (`validation/reference/*/README.md`) |
| `time_profile_vs_reference.py` | a new-model dump | G(t) of e_aq, °OH, H2O2, H2 at 1 ns–1 µs against `sbs_water`; it showed that the gap opens after 100 ns |
| `merge_block_dumps.py` | `<src> <dst> <nblocks>` (dumps in `<src>/blk<i>`) | One pooled dump: sums of `sumG`, `sumG2`, `nEvent`, `SpeciesMeso` counts, energy and events |
| `serial_vs_mt_blockmeans.py` | `<serial_dir> <mt_dir>` (5 blocks each) | Batch-means SE per species and the Serial vs `--threads 4` tolerance (3 × max relative SE of the difference = 0.0332) |
| `plot_long_runs.py` | `<new_dump> <ref_dump> <out.png> <title>` | `docs/irt-syn-meso-long-{water,o2}.png` (G(t) to 1 s) |
| `meso_decay_rate.py` | a dump with bulk O2 | Effective decay rate of e_aq in `SpeciesMeso` against k[O2]; this exposed the stale late records (fixed in `baf4b29`) |
| `meso_totals_check.py` | a dump and its Debug log | Recorded `SpeciesMeso` total at the end time against the true mesh total (25796 = 25796 after `baf4b29`) |

The gate comparisons themselves (new model vs SBS, Serial vs MT on the pooled dumps) were run with `analysis/compare_reference.py`.

Not kept: the Windows thread-stack and debugger helpers used to locate the O2 hang (`07b-stack*.py`, `07b-resume.py`, `07b-nearest.py`) and a code-edit helper. They are debugging tools, not analysis. Their findings are in the commit message of `e0a5b88`.
