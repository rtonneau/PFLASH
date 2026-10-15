# Ticket 04: manifest-dump

**Acceptance Criteria:**
- [ ] `OutputDir::GetDirectory()` returns the configured directory (empty if none) and is covered in `OutputDirTest`.
- [ ] `RunManifest::Write(prefix, subdir, files)` fills `ManifestData::Manifest` (timestamp, Geant4 version, macro, Chemistry, scavengers, pH, chemistry end time, run mode, threads, output dir as configured and absolute, prefix, subdir, files, runs) and writes `Manifest.json` through `OutputDir::Resolve`; a failed open raises a `JustWarning` `G4Exception`.
- [ ] `WriteAllAndReset` records each data file it wrote (prefix included, manifest excluded), calls `RunManifest::Write` before `ClearAccumulated()`, and no longer writes `EnergyDeposit.Txt`.
- [ ] `sim.cc` passes the macro name via `RunManifest::SetMacroName`.
- [ ] Smoke run (Serial and `--threads 2`): first dump's manifest has two runs (10 keV / 2 events; 25 keV / 1 event with a different seed) and `"totalEvents": 3`; the `second/` subfolder dump has one run; no `EnergyDeposit.Txt`; every name in `files` exists on disk; the exit-time flush also writes `EndOfRun_Manifest.json` when a run is left undumped.

**Files to Touch:**
- `header/core/OutputDir.hh`, `src/core/OutputDir.cc`, `test/OutputDirTest.cc`
- `header/scoring/RunManifest.hh` (create), `src/scoring/RunManifest.cc` (create)
- `src/scoring/RunAccumulatorMessenger.cc`
- `sim.cc`

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja --output-on-failure
cmake --build build
cd build && ./sim manifest_smoke.in --dir smoke/m1 && cat smoke/m1/Manifest.json
```

Expected:
All unit tests `Passed`; build succeeds; the manifest shows the runs described above and the markers from `run.successMarkers` appear in the output. Repeat with `--threads 2 --dir smoke/m2`; beam fields match.

**Notes:**

Plan Task 4 lists the collector fields and the smoke macro (`manifest_smoke.in`, written into `build/macro`, untracked, ends with a dump). `G4VERSION_TAG` needs its `"$Name: "` prefix and `" $"` suffix stripped. Use `dynamic_cast<G4MTRunManager*>` for the run mode and thread count. Follow the run/build procedure in `.claude/geant4-instructions.md`.
