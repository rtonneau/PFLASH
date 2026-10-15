# Ticket 03: sbs-max-time-step

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `/chem/sbs/maxTimeStep <value> <unit>` (PreInit, `DeclareMethodWithUnit`, default unit ns) is on the same `/chem/sbs/` messenger. A value <= 0 is a fatal `G4Exception` at the command. Not issued means no cap.
- [ ] `DnaChemistryList` exposes `G4double GetMaxTimeStep() const` (returns `DBL_MAX` when unset) and `void ApplyMaxTimeStep() const`, which calls `G4Scheduler::Instance()->SetMaxTimeStep(...)` only when a cap was set. `RunAction::BeginOfRunAction` calls it right next to `ApplyReactionTimeBinning()`, on every thread that runs chemistry.
- [ ] Manifest `sbs.maxTimeStep_ns` = the cap in ns, or `null` when unset.
- [ ] CLAUDE.md Macro section and the sim-output skill's Manifest keys are updated.
- [ ] A scratch run of `beam_boscolo.in` + `/chem/sbs/maxTimeStep 1 ns` exits 0 and its Manifest shows `"maxTimeStep_ns": 1`. The plain `beam_boscolo.in` shows `null`.
- [ ] ctest is still all green.

**Files to Touch:**
- `header/chemistry/DnaChemistryList.hh`
- `src/chemistry/DnaChemistryList.cc`
- `src/actions/RunAction.cc`
- `src/scoring/RunManifest.cc`
- `CLAUDE.md`
- `.claude/skills/sim-output/SKILL.md`

**Verification Step:**

Run:
```bash
cmake --build build --target sim && cd build && ./sim.exe <scratch copy of beam_boscolo.in with /chem/sbs/maxTimeStep 1 ns> --dir ../.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t03 && grep -A3 '"sbs"' ../.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t03/Manifest.json
```

Expected:
Exit 0, `"maxTimeStep_ns": 1`.

**Notes:**

`G4Scheduler::SetMaxTimeStep` sets `fMaxTimeStep`, which `G4Scheduler::Stepping()` copies into `fTimeStep` at the start of every step (`G4Scheduler.cc:499`). `G4Scheduler` is thread-local, which is why it is applied in `BeginOfRunAction` and not in the PreInit command. `TimeStepAction`'s `AddTimeStep` entries are minimums; leave them alone.
