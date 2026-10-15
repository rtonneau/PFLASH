# Ticket 04: dump-integration-and-docs

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `RunAccumulatorMessenger::DumpAndReset` calls `MesoSpatialFile::MoveStaged(OutputDir::GetDirectory(), OutputDir::Resolve(MesoSpatialFile::FileName()), moved, err)` right after the pre-chemical move. When moved, it adds `prefix + "SpeciesMesoSpatial.h5"` to `files` (so it is listed in `Manifest.json`). A failure is a `JustWarning`.
- [ ] Works with `/run/dumpDataAndReset <prefix>`, with `/run/dumpDataAndResetToDir <subdir>`, and with the `EndOfRun_` safety net (`FlushIfPending`).
- [ ] `RunManifest::Write` adds `mesoSpatialOutput` (bool) next to the other meso model keys.
- [ ] New example macro `macro/beam_meso_spatial.in`: 10 keV e-, `/chem/meso/spatialOutput true`, `/scheduler/endTime 1 ms`, `/run/beamOn 2`, `/run/dumpDataAndReset`.
- [ ] Docs updated: `CLAUDE.md` (Key Files: `MesoSpatialFile.cc`, the switch in Macro and Logging, the output paragraph), `.claude/skills/sim-output/SKILL.md` (file format and layout, how to compute concentration), and `run.outputs` in `.claude/.claude-project.json`.

**Files to Touch:**
- `src/scoring/RunAccumulatorMessenger.cc`
- `src/scoring/RunManifest.cc`
- `macro/beam_meso_spatial.in`
- `CLAUDE.md`
- `.claude/skills/sim-output/SKILL.md`
- `.claude/.claude-project.json`

**Verification Step:**

Run (MSVC env):
```bash
cmake --build build --config RelWithDebInfo --target sim && cd build && ./sim beam_meso_spatial.in --dir ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t04 && ls ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t04 && grep -n "SpeciesMesoSpatial\|mesoSpatialOutput" ../.scratch/tests/2026-10-03__export-mesoscopic-species-concentrations/t04/Manifest.json
```

Expected:
`SpeciesMesoSpatial.h5` sits beside `SpeciesMeso.csv`, `.pending_meso_spatial/` holds no file, `Manifest.json` lists the file and `"mesoSpatialOutput": true`, and the log shows `[RunAccumulatorMessenger] dumped and reset`.

**Notes:**

The `Resolve` target already carries the prefix and subdir. Keep the docs short and in the style of the surrounding entries.
