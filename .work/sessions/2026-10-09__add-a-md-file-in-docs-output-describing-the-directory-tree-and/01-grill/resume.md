# Session: Add a md file in docs/output/ describing the directory tree and files created by a typical simulation run (both with and without mesoscopic)

**Date:** 2026-10-09T06:49:38.290Z
**Status:** Grill phase complete

## Problem Statement

There is no single place describing the directory tree and files a typical `sim` run leaves on disk. The information is spread across the `sim-output` skill, `CLAUDE.md` and `SpeciesMesoSpatial-h5.md`. Add documentation under `docs/output/` that shows the trees and explains each file, for runs with and without mesoscopic output.

## Context & Constraints

- **Current behavior:** Output is described in prose in `.claude/skills/sim-output/SKILL.md`; only `docs/output/SpeciesMesoSpatial-h5.md` exists in `docs/output/`. No tree views.
- **Pain point:** A reader cannot see at a glance what a run produces under each output layout (cwd, prefix, subfolder, safety-net flush, staging folders).
- **Dependencies:** The doc must match the real output: file names come from a real smoke run, not from memory. Keep the existing format doc and skill unchanged except for one link line each.
- **Tech stack:** Markdown only; `sim` run (10 keV e-, `/run/beamOn 2`, 1 ms end time) to capture trees.

## Success Metrics

- `docs/output/README.md` exists and has one line per output file: content, writer, when it appears, typical size, with links to the `sim-output` skill and `SpeciesMesoSpatial-h5.md`.
- `docs/output/layouts/` has one file per layout (cwd, prefix, subdir with `--dir` and `/run/dumpDataAndResetToDir`, `EndOfRun_` flush, staging folders). Each shows real, trimmed trees for three meso cases: default, `spatialOutput true`, end time <= hand-over time.
- Every file name in the trees appears in a real run's output. One link line to the new docs is added in the `sim-output` skill, `CLAUDE.md` and `SpeciesMesoSpatial-h5.md`.

## Architecture & Approach

Documentation only, no code change. Run `sim` in the scratch dir for each layout and meso case, capture the trees, trim repetitive entries (for example `PreChemical_run<R>_event<E>.txt` shown once with `...`), and write the index and layout files. Per-file descriptions are one line each; formats stay in the skill and the HDF5 doc.

## Assumptions & Trade-offs

- "Without mesoscopic" is interpreted as covering all three cases: the default run, the spatial-output run and the no-mesoscopic-stage run (end time <= hand-over time).
- Not documenting column or header layouts of the Txt/csv files (the skill keeps behavior details).
- No code, macro or output-format change.

## Open Questions

- What an end time <= hand-over time run actually writes (`SpeciesMeso.*` empty or missing) is to be checked in the real run.

## Notes

Files: `docs/output/README.md` (index) and `docs/output/layouts/*.md`. Link lines go in `.claude/skills/sim-output/SKILL.md`, `CLAUDE.md` and `docs/output/SpeciesMesoSpatial-h5.md`.
