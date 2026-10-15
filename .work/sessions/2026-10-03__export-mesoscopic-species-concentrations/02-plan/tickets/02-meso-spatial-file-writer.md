# Ticket 02: meso-spatial-file-writer

**Model:** sonnet

**Acceptance Criteria:**
- [ ] New portable `MesoSpatialFile` (std + HDF5 C++ only, no Geant4, no other dnachem-min class), with this exact API:
  ```cpp
  namespace MesoSpatialFile {
  struct Snapshot {                      // one mesh state
    double cellSize_nm = 0.;
    std::vector<double> position_nm;     // N*3, row-major, cell centres
    std::vector<std::uint32_t> counts;   // N*S, row-major, column order = species
  };
  struct Record { double time_ns; std::size_t snapshot; };  // index into snapshots
  struct EventData {
    int runId = 0; int eventId = 0;
    std::vector<Snapshot> snapshots;     // distinct mesh states
    std::vector<Record> records;         // one per record time, ascending
  };
  constexpr int kFormatVersion = 1;
  std::string FileName();                              // "SpeciesMesoSpatial.h5"
  std::string StagingDir(const std::string& outputDir); // outputDir + "/.pending_meso_spatial", or ".pending_meso_spatial"
  std::string StagedPath(const std::string& outputDir); // StagingDir + "/" + FileName()
  bool AppendEvent(const std::string& path, const std::vector<std::string>& species,
                   const EventData& event, std::string& err);
  bool MoveStaged(const std::string& outputDir, const std::string& target,
                  bool& moved, std::string& err);   // no staged file: true, moved=false
  }
  ```
- [ ] `AppendEvent` creates the staging folder and the file when missing. On creation it writes these root attributes: `species` (string array), `formatVersion` = 1, `units` = "position_nm: nm (cell centre, world frame); cellSize_nm: nm (cell side); time_ns: ns; counts: molecules per cell".
- [ ] If the file exists with a different `species` attribute, or `/run<R>/event<E>` already exists, `AppendEvent` returns false with a message and leaves the file unchanged.
- [ ] Layout: `/run<R>/event<E>/snapshot<k>/` for each record k (k from 0, in record order), with attributes `time_ns` and `cellSize_nm` and datasets `position_nm` (N×3 float64) and `counts` (N×S uint32), chunked and gzip level 4. When records k and j share a `snapshot` index, the later group's two datasets are HDF5 hard links to the first group's datasets, while each group keeps its own `time_ns`.
- [ ] Every HDF5 call is under one function-static `std::mutex`, and the file is closed before returning.
- [ ] `test/MesoSpatialFileTest.cc` round-trips two events (one with N = 0 cells, one with two records sharing a snapshot). It reads back with H5Cpp, asserts values, attributes and the hard link (`H5Oget_info` addresses equal), checks the species-mismatch and duplicate-event errors, and checks `MoveStaged` (moved file, then a no-file call).

**Files to Touch:**
- `header/scoring/MesoSpatialFile.hh`
- `src/scoring/MesoSpatialFile.cc`
- `test/MesoSpatialFileTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run (MSVC env):
```bash
cmake -S . -B build-ninja && cmake --build build-ninja --target MesoSpatialFileTest && ctest --test-dir build-ninja -R MesoSpatialFileTest --output-on-failure
```

Expected:
`100% tests passed, 0 tests failed out of 1`

**Notes:**

- CMake: `add_executable(MesoSpatialFileTest test/MesoSpatialFileTest.cc src/scoring/MesoSpatialFile.cc)` + `target_link_libraries(... hdf5::hdf5-shared hdf5::hdf5_cpp-shared)`. No Geant4 is needed.
- Test files go in a temp dir under the test's working dir; remove them at the end.
- Use `NDEBUG`-safe asserts (see `.claude/geant4-instructions.md` section 5).
- `MoveStaged` removes an existing target first (same as `PreChemicalFiles::MoveStaged`).
- Do not put a mutex in the header.
