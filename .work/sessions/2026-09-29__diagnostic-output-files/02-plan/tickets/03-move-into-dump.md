# Ticket 03: move-into-dump

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `RunAccumulatorMessenger::WriteAllAndReset`, after the PhysicsInteractions files and before `RunManifest::Write`:
  - calls `PreChemicalFiles::MoveStaged(StagingDir(OutputDir::GetDirectory()), [](const std::string& n){ return OutputDir::Resolve(n); })` (the prefix and subdir are already set there);
  - appends `result.moved` to `files`;
  - raises one `G4Exception("RunAccumulatorMessenger::WriteAllAndReset", "PreChemicalMoveFailed", JustWarning, msg)` per failure;
  - logs the moved count with `DnaLogger` Info.
- [ ] `.claude/.claude-project.json` `run.outputs`: the `output_event_…` entry is replaced by `"PreChemical_run<R>_event<E>.txt (one per event, moved into the Dump)"`.
- [ ] `.claude/skills/sim-output/SKILL.md`: the `output_event` lines are replaced by the staging, move and manifest-listing behaviour.
- [ ] `CLAUDE.md`: the `RunAccumulatorMessenger.cc` entry mentions the pre-chemical files, and no `output_event` mention remains.
- [ ] The Serial and MT smoke runs pass.

**Files to Touch:**
- `src/scoring/RunAccumulatorMessenger.cc`
- `.claude/.claude-project.json`
- `.claude/skills/sim-output/SKILL.md`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
# build/macro/prechem_dump.in: 10 keV e-, /run/beamOn 2, /run/beamOn 1, /run/dumpDataAndResetToDir d1
cd build && ./sim prechem_dump.in --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t3s
# build/macro/prechem_mt.in: 10 keV e-, /run/beamOn 4, /run/dumpDataAndReset mt_
./sim prechem_mt.in --threads 2 --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t3m
```

Expected:
- **Serial:**
  - `t3s/d1/` holds `PreChemical_run0_event0.txt`, `PreChemical_run0_event1.txt` and `PreChemical_run1_event0.txt`.
  - `d1/Manifest.json` `files` lists them in that order.
  - `t3s/.pending_prechem/` is empty, and there are no `EndOfRun_` files.
- **MT:**
  - Exactly `mt_PreChemical_run0_event0.txt` through `mt_PreChemical_run0_event3.txt` in `t3m/`, all listed in `mt_Manifest.json`.
  - `.pending_prechem/` is empty.
  - No `PreChemicalMoveFailed` warning.

**Notes:**

- `WriteAllAndReset` is the single path for `/run/dumpDataAndReset`, `/run/dumpDataAndResetToDir` and `FlushIfPending`. Change nothing else there.
- Use the `sim-output` skill for the output-format wording.
