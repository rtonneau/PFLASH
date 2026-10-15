# Ticket 03: macro-resolver

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.49)
**Effort:** medium

**Acceptance Criteria:**
- [ ] `MacroResolver::Resolve(arg, exeDir)` returns the first existing of: `arg` as given, `<exeDir>/<arg>`, `<exeDir>/macro/<arg>`; on failure returns the three tried paths for the error message.
- [ ] Pure logic (standard library only), portable like `ArgParser`; covered by `test/MacroResolverTest.cc` registered in CMake.
- [ ] `sim.cc` uses it, finds the exe directory on Windows and Linux, prints the resolved path, and exits with a clear message when nothing is found; the default macro stays `beam.in`.
- [ ] `RunManifest::SetMacroName` receives the resolved path.

**Files to Touch:**
- `header/core/MacroResolver.hh`
- `src/core/MacroResolver.cc`
- `test/MacroResolverTest.cc`
- `CMakeLists.txt`
- `sim.cc`

**Verification Step:**

Run:
```bash
cd build-ninja && ctest -R MacroResolver --output-on-failure; cd ../build && ./sim does_not_exist.in; echo exit=$?
```

Expected:
Test passes; the second command lists the three tried paths and exits non-zero.

**Notes:**

`sim.cc` currently builds `"macro/" + argv[1]`. Do not change how `--threads`/`--dir` are parsed. Use `std::filesystem` for the checks only inside the resolver.
