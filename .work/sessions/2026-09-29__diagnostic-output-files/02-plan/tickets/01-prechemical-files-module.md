# Ticket 01: prechemical-files-module

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `header/scoring/PreChemicalFiles.hh` and `src/scoring/PreChemicalFiles.cc` exist, standard library only, namespace `PreChemicalFiles`, with:
  - `std::string StagedFileName(int runId, int eventId)` returning `"PreChemical_run<R>_event<E>.txt"`
  - `bool ParseStagedFileName(const std::string& name, int& runId, int& eventId)`, true only for an exact match of that pattern
  - `std::string StagingDir(const std::string& outputDir)`: `outputDir + "/.pending_prechem"`, or `".pending_prechem"` when `outputDir` is empty
  - `bool EnsureStagingDir(const std::string& outputDir, std::string& err)`, using `create_directories`
  - `struct MoveResult { std::vector<std::string> moved; std::vector<std::string> failures; };`
  - `MoveResult MoveStaged(const std::string& stagingDir, const std::function<std::string(const std::string&)>& resolveTarget)`
- [ ] `MoveStaged` handles only regular files that `ParseStagedFileName` accepts, in numeric (run, event) order. It removes an existing target, then renames. `moved` gets the target's filename (so the prefix is included). A failure adds a message to `failures` and leaves the file staged. A missing staging dir returns empty lists.
- [ ] `test/PreChemicalFilesTest.cc` passes with these tests:
  - `StagedFileNameFormat`: `StagedFileName(0, 12) == "PreChemical_run0_event12.txt"`
  - `ParseRoundTrip`: parsing gives 0 and 12. It rejects `"PreChemical_run0_event12.txt.bak"`, `"PreChemical_runX_event1.txt"` and `"notes.txt"`.
  - `StagingDirJoin`
  - `MoveStagedOrdersNumerically`: staged `run0_event2`, `run0_event10` and `run1_event0` with the resolver `<tmp>/dump/p_<name>` give `moved == {"p_PreChemical_run0_event2.txt","p_PreChemical_run0_event10.txt","p_PreChemical_run1_event0.txt"}` and an empty staging folder.
  - `MoveStagedOverwritesTarget`: the target held "old" and now holds the staged content; no failures.
  - `MoveStagedIgnoresForeignFiles`: `notes.txt` stays and is not listed.
  - `MoveStagedMissingDirIsEmpty`
  - `MoveStagedKeepsEmptyFile`
- [ ] The file header comment says the module is portable and cites the ADR 0005 addendum.

**Files to Touch:**
- `header/scoring/PreChemicalFiles.hh`
- `src/scoring/PreChemicalFiles.cc`
- `test/PreChemicalFilesTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
cmake --build build-ninja --target PreChemicalFilesTest && ctest --test-dir build-ninja --output-on-failure
```

Expected:
Every test Passed, none Not Run (build every test target first if ctest lists Not Run).

**Notes:**

- Write the tests first (TDD) in the style of `test/OutputDirTest.cc`, using a temp folder under `std::filesystem::temp_directory_path()`.
- CMake wiring goes next to `RunAccumulatorTest`: `add_executable(PreChemicalFilesTest test/PreChemicalFilesTest.cc src/scoring/PreChemicalFiles.cc)`, the same include and link lines as `OutputDirTest`, and `add_test(NAME PreChemicalFilesTest COMMAND PreChemicalFilesTest)`.
- `sim` picks up the source through the existing `GLOB_RECURSE`.
- See `.claude/geant4-instructions.md` section 5 for the `NDEBUG` and Debug-CRT pitfalls.
