# Ticket 02: rename-executable-pflash

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.64)
**Effort:** medium

**Acceptance Criteria:**
- [ ] `sim.cc` is `pflash.cc` (git mv); CMake `project()` is `PFLASH`, the executable target is `pflash`, the `Simulation` custom target is renamed accordingly, and the macro copy step uses `$<TARGET_FILE_DIR:pflash>`.
- [ ] `.claude/.claude-project.json` (`executable`, CLI syntax, outputs) and `.claude/geant4-instructions.md` say `pflash`.
- [ ] Source comments and tests that refer to `sim.cc`/`sim` are updated.
- [ ] `build/` and `build-ninja/` configure from scratch, build, and all ctest tests pass.

**Files to Touch:**
- `sim.cc` → `pflash.cc`, `CMakeLists.txt`
- `.claude/.claude-project.json`, `.claude/geant4-instructions.md`
- `src/actions/RunAction.cc`, `header/core/ArgParser.hh`, `header/core/OutputDir.hh`, `header/scoring/RunAccumulatorMessenger.hh`, `header/scoring/RunManifest.hh`, `test/ArgParserTest.cc`, `test/RunAccumulatorTest.cc`

**Verification Step:**

Run:
```bash
grep -rn "sim\.cc\|\bsim\.exe\|\./sim\b" --include=*.cc --include=*.hh --include=*.txt --include=*.json --include=*.md . --exclude-dir=.git --exclude-dir=.work | head
```
then follow `.claude/geant4-instructions.md` to build `build/` and run ctest in `build-ninja/`.

Expected:
No hits in code/config (kept history docs may still mention `sim`); builds succeed, all tests pass.

**Notes:**

Mind the `\bsim\b` word: do not touch words like "simulation". Delete stale CMake caches in `build/` and `build-ninja/` before reconfiguring. The Windows MSVC environment steps are in `.claude/geant4-instructions.md`. Leave CLAUDE.md prose for ticket 5.
