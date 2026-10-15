# Ticket 04: dump-integration-and-docs

**Status:** ✅ Done

## Local Test Result

`./sim beam_meso_spatial.in --dir ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t04r` (rerun in this session): exit 0 with `[RunAccumulatorMessenger] dumped and reset (prefix='', subdir='')`. `SpeciesMesoSpatial.h5` sits beside `SpeciesMeso.csv` and `.pending_meso_spatial/` is empty. `Manifest.json` has `"mesoSpatialOutput": true` and lists `SpeciesMesoSpatial.h5` in `files`. The subagent also checked `/run/dumpDataAndResetToDir sub1` (the h5 goes to `sub1/` and is listed in that manifest) and the `EndOfRun_` safety net (`EndOfRun_SpeciesMesoSpatial.h5` written and listed), both with exit 0.

## Review Notes

Implemented by a sonnet subagent. All criteria hold: MoveStaged runs right after the pre-chemical move and uses `OutputDir::Resolve` for the target (prefix and subdir), the file is added to `files` only when moved, a failed move is a `MesoSpatialMoveFailed` JustWarning, `mesoSpatialOutput` is in the manifest, the example macro exists, and CLAUDE.md, the sim-output skill and `run.outputs` are updated. After review: added `/chem/meso/spatialOutput` to the `MesoMessenger.cc` Key Files line in CLAUDE.md.

## Blockers / Challenges

None.

## Commits

- 99629c9 feat(scoring): move SpeciesMesoSpatial.h5 into each dump, manifest key, docs (ticket 04)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
