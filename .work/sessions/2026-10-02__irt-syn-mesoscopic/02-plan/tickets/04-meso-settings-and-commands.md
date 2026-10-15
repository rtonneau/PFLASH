# Ticket 04: meso-settings-and-commands

**Model:** sonnet

**Acceptance Criteria:**
- [ ] New pure module `header/chemistry/MesoSettings.hh` / `src/chemistry/MesoSettings.cc` (standard library only, unit-agnostic doubles):
  - `int PixelCount(double boxSide, double targetVoxel, int maxPixels = 65536)`: the power of 2 `p ≥ 1` minimising `|boxSide/p − targetVoxel|`, ties going to the larger `p`, then capped at `maxPixels`. The cap avoids the Geant4 `G4DNAMesh::ConvertIndex` int overflow found in ticket 02. `bool PixelCountCapped(double boxSide, double targetVoxel, int maxPixels = 65536)` is true when the cap applied.
  - `std::vector<double> LogTimeGrid(double start, double end, int perDecade)`: `start`, then `start·10^(k/perDecade)` for every value `< end`, then `end`. The values are strictly increasing. `start ≥ end` or `perDecade < 1` throws `std::invalid_argument`.
- [ ] `test/MesoSettingsTest.cc`, wired in `CMakeLists.txt` like `ScavengerSpecTest`, asserts:
  - `PixelCount(3.2, 0.00625) == 512`;
  - `PixelCount(1.0, 1.0) == 1`;
  - `PixelCount(1000.0, 0.00625) == 65536` and `PixelCountCapped(1000.0, 0.00625)` is true;
  - `PixelCount(400.0, 0.00625) == 65536` and `PixelCountCapped(400.0, 0.00625)` is false;
  - `PixelCount(1000.0, 0.00625, 1024) == 1024`;
  - `LogTimeGrid(5, 50, 10)` has 11 values, from 5 to 50;
  - `LogTimeGrid(5, 7, 10)` ends at 7;
  - the invalid inputs throw.
- [ ] PreInit commands on a new `MesoMessenger`, registered in a new `/chem/meso/` directory:
  - `/chem/meso/handOverTime <value> <unit>`: must be > 0, default 5 ns;
  - `/chem/meso/voxelSize <value> <unit>`: must be > 0, default 6.25 nm;
  - `/chem/meso/timesPerDecade <int>`: must be ≥ 1, default 10.
  
  Invalid values are a fatal `G4Exception` at the command. The settings are owned by a single process-wide object read by every worker's `TimeStepAction`. Ticket 02's hard-coded 5 ns and 6.25 nm are replaced by these settings, and the pixel count comes from `MesoSettings::PixelCount`. When `PixelCountCapped` is true, a `DnaLogger` Warning (once, master only) gives the requested and the actual cell size (on the default 1 mm box: 6.25 nm requested, 15.26 nm used).
- [ ] The end-time default in `ActionInitialization::Build()` becomes `1 * s`. `/scheduler/endTime` after `/run/initialize` still overrides it.
- [ ] A run with `/chem/meso/handOverTime 10 ns` and `/chem/meso/voxelSize 12.5 nm` logs a hand-over at 10 ns and the corresponding pixel count. All tests pass.

**Files to Touch:**
- `header/chemistry/MesoSettings.hh`, `src/chemistry/MesoSettings.cc`, `test/MesoSettingsTest.cc`, `CMakeLists.txt`
- `header/chemistry/MesoMessenger.hh`, `src/chemistry/MesoMessenger.cc`
- `src/chemistry/TimeStepAction.cc`, `header/chemistry/TimeStepAction.hh`
- `src/actions/ActionInitialization.cc`, `sim.cc` (only if the messenger is created there)

**Verification Step:**

Run (PowerShell, MSVC env):
```bash
cmake --build build-ninja --target MesoSettingsTest; ctest --test-dir build-ninja --output-on-failure
cmake --build build --target sim
cd build; ./sim 04-cmds.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/04-cmds > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/04-cmds.log 2>&1
grep -iE "hand-?over|pixel" ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/04-cmds.log
```

Expected:
`MesoSettingsTest` fails first on an assertion (red step), then all tests pass. The run exits 0, and the log shows the hand-over at 10 ns and the pixel count for 12.5 nm cells.

**Notes:**

TDD with plain `assert` and the MSVC CRT block (`.claude/geant4-instructions.md` §5). Keep `MesoSettings` portable like `ChemistryRegistry.cc`. Use `G4UIcmdWithADoubleAndUnit` / `G4UIcmdWithAnInteger` on a `G4UImessenger`, the way `ChemistrySelectMessenger` attaches to an existing directory without a `G4GenericMessenger`. The record-time grid (`LogTimeGrid`) is consumed by ticket 05.
