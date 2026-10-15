# Ticket 05: species-meso-output

**Model:** opus

**Acceptance Criteria:**
- [ ] New pure class `MesoSpeciesCounter` (`header/scoring/MesoSpeciesCounter.hh`, `src/scoring/MesoSpeciesCounter.cc`, standard library only) with these methods:
  - `void Add(double time, const std::string& species, long count)`;
  - `void Merge(const MesoSpeciesCounter& other)`;
  - `void Clear()`;
  - `bool Empty() const`;
  - `void WriteAscii(std::ostream&, long events) const`, writing per time: the time, then one `<species> <mean count per event>` line per species, sorted;
  - `void WriteCsv(std::ostream&) const`, writing the header `time_ns,species,count` with one row per (time, species) holding the summed count.
  
  It is the same shape as `PhysicsInteractionCounter`.
- [ ] `test/MesoSpeciesCounterTest.cc` (wired in `CMakeLists.txt`) covers:
  - Add then Merge sums per (time, species);
  - Clear empties it;
  - the CSV header and row order;
  - the ASCII means for 2 events.
- [ ] Per event, `TimeStepAction` registers `MesoSettings::LogTimeGrid(handOver, endTime, timesPerDecade)` with `AddTimeToRecord`, then copies `GetCounterMap()` into the worker `Run`'s `MesoSpeciesCounter`. It skips bulk species (user ID ending in `(B)`, or `H2O`) and uses display names, like `Species.Txt`. It then resets the scheduler counter.
- [ ] `Run::Merge` merges it, and `RunAccumulator` owns the persistent copy. `/run/dumpDataAndReset` writes `SpeciesMeso.Txt` and `SpeciesMeso.csv` through `OutputDir::Resolve`, with the dump prefix applied, lists them in `Manifest.json`, and clears them. The `EndOfRun_` safety-net flush includes them.
- [ ] `RunAccumulatorTest` is extended for the new counter. All tests pass.
- [ ] *Added after ticket 02:* the molecule counter is muted from the hand-over to the end of the event, so `G4MoleculeCounter` holds frozen hand-over counts at later times. `ScoreSpecies` therefore writes `Species.*` time points only up to the hand-over time (inclusive). No `Species.*` row has a time later than the hand-over.
- [ ] Runs (10 keV × 2, end time 1 ms, PureWater) are made both Serial and with `--threads 2`. Each dump has a non-empty `SpeciesMeso.Txt/.csv` whose first time equals the hand-over and last time equals the end time, with no `(B)` or water species, and is listed in `Manifest.json`. Serial and MT totals per species at the last time agree within 3σ (Poisson).

**Files to Touch:**
- `header/scoring/MesoSpeciesCounter.hh`, `src/scoring/MesoSpeciesCounter.cc`, `test/MesoSpeciesCounterTest.cc`, `CMakeLists.txt`
- `src/chemistry/TimeStepAction.cc`, `header/chemistry/TimeStepAction.hh`
- `src/scoring/Run.cc`, `header/scoring/Run.hh`
- `src/scoring/RunAccumulator.cc`, `header/scoring/RunAccumulator.hh`, `test/RunAccumulatorTest.cc`
- `src/scoring/RunAccumulatorMessenger.cc`, `src/scoring/RunManifest.cc`

**Verification Step:**

Run (PowerShell, MSVC env):
```bash
cmake --build build-ninja; ctest --test-dir build-ninja --output-on-failure
cmake --build build --target sim
cd build; ./sim 05-meso.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-serial > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-serial.log 2>&1
./sim 05-meso.in --threads 2 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-mt > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-mt.log 2>&1
head -5 ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-*/SpeciesMeso.csv; grep -c "(B)" ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/05-*/SpeciesMeso.Txt
```

Expected:
All tests pass, with a red step first for the new test. Both runs exit 0. Each `SpeciesMeso.csv` starts with `time_ns,species,count`, and its time range runs from the hand-over to the end time. The `(B)` counts are `0`, and `Manifest.json` lists both files.

**Notes:**

Consumes `MesoSettings::LogTimeGrid` and the meso settings object from ticket 04. Copy the existing output pattern from `PhysicsInteractionCounter` (pure class) and its path through `Run` → `RunAccumulator` → `RunAccumulatorMessenger` → `RunManifest`. Read the `sim-output` skill (`.claude/skills/sim-output/SKILL.md`) before changing output code. UHDR reads the counter map in `Scorer.cc` (~l.474–548) and calls `ResetCounter()` afterwards.
