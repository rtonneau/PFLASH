# Implementation Plan

**Session:** Add a md file in docs/output/ describing the directory tree and files created by a typical simulation run (both with and without mesoscopic)
**Date:** 2026-10-09T07:25:07.313Z
**Estimated effort:** 2-3 hours

## Strategy

Documentation only. First capture real output trees from smoke runs and write the index of output files (`docs/output/README.md`). Then write one file per layout under `docs/output/layouts/`, using the captured trees. Last, add the link lines to the existing docs.

## Tickets Overview

- **Ticket 1:** Run `sim` for the three meso cases and the layouts, save the raw trees in the scratch dir, and write `docs/output/README.md` (one line per output file).
- **Ticket 2:** Write the per-layout files in `docs/output/layouts/` (cwd, prefix, subdir, EndOfRun_ flush, staging folders), each with trimmed real trees for the three meso cases.
- **Ticket 3:** Add one link line each in the `sim-output` skill, `CLAUDE.md` and `SpeciesMesoSpatial-h5.md`.

## Sequencing Rationale

The index defines the file descriptions the layout files refer to, and both need the real trees from the runs. Link lines go last so they never point at missing files.

## Risks & Mitigation

- **Risk:** A full 1 s end time is slow, so trees might not match a normal run. → **Mitigation:** Use `/scheduler/endTime 1 ms` after `/run/initialize`; file names do not depend on the end time.
- **Risk:** The no-mesoscopic-stage case (end time <= hand-over) may write nothing or fail. → **Mitigation:** Check the real behavior and document exactly what appears.

## Assumptions

- The `build/` run build is available, following `.claude/geant4-instructions.md`.
- Raw run output stays in `.scratch/` and is not committed.
