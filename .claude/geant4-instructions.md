# Geant4 app: build, run, test

Reusable across Geant4 projects. Nothing here is project-specific: values come
from the `build`, `run` and `test` sections of `.claude/.claude-project.json`
(paths resolved per `~/.claude/CONFIG_RESOLUTION.md`). Keys are written
`build.runBuildDir` etc.

If a section or key is missing, discover it instead of guessing: read
`CMakeLists.txt` (targets, `add_test`, CMake options) and `<dir>/CMakeCache.txt`
(`CMAKE_GENERATOR`, `CMAKE_BUILD_TYPE`, `CMAKE_TOOLCHAIN_FILE`, `Geant4_DIR`),
then offer to write what you found back into the config.

## 1. Shell environment (Windows / MSVC)

Configure works in any shell, but compiling needs the MSVC x64 environment or
it fails on missing STL headers (`cstddef`, `complex`). Load it in the same
command as the build. Use the PowerShell tool; the Bash tool cannot run this.

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && <command>"
```

If `vswhere` fails or returns nothing in the PowerShell tool, call
`vcvars64.bat` by its full path instead (machine-specific; on the current
dev machine: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`):

```powershell
cmd /c "`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" >nul && <command>"
```

Linux/macOS: skip this section. Tool calls time out at 2 min: run full builds
and simulations with `run_in_background`, and read the log they write.

## 2. Configure and build

Out-of-source, one directory per purpose (see section 5 for why there are two):

```
cmake -S . -B <dir> -G <build.generator> -DCMAKE_BUILD_TYPE=<type> \
      -DGeant4_DIR=<geant4InstallPath>/lib/cmake/Geant4 \
      -DCMAKE_TOOLCHAIN_FILE=<build.toolchainFile>   # only if set
cmake --build <dir> --config <type> --target <build.executable>
```

- `<dir>`/`<type>` = `build.runBuildDir`/`build.runBuildType` for the app,
  `build.testBuildDir`/`build.testBuildType` for tests.
- Do not re-run configure on an existing directory unless CMake asks; an
  incremental `--build` is enough.
- Sources are usually globbed with `CONFIGURE_DEPENDS`: a new `.cc`/`.hh` is
  picked up by the next build (expect a "GLOB mismatch, re-running CMake" line).
- Build errors about undefined Geant4 symbols mean a missing `find_package`
  component; the first suspect for MSVC header errors is section 1.
- `LNK1201: error writing to program database '<exe>.pdb'` (a stale or locked
  PDB): delete `<dir>/<exe>.pdb` and `<dir>/<exe>.exe`, then rebuild. No code
  cause.

## 3. Run

- Run from `run.workingDir` (the run build dir), executable
  `build.executable` (+ `.exe` on Windows).
- Windows: run the executable from the PowerShell tool, not Git Bash (it does
  not start from Git Bash). If it exits at once with `0xC0000135` (a DLL not
  found, typically Qt), prepend the directory holding those DLLs to `PATH` in
  the same command. Machine-specific; on the current dev machine:
  ```powershell
  $env:PATH = "E:\miniconda3\envs\qt5\Library\bin;$env:PATH"; .\sim.exe <macro> > run.log 2>&1
  ```
- Macro argument: see `run.cli.syntax` and `run.macroArgIsFilenameOnly`.
- If `run.macroCopiedAtBuild`, macros are copied into the build dir only when
  the executable target builds. After editing a macro, rebuild (or copy it).
  For throwaway experiments write a scratch macro into `<runBuildDir>/macro`
  instead of touching the tracked ones.
- Isolate each run's output with `--dir` (if the app supports it) and delete
  scratch output afterward.
- Long runs: start in the background, redirect to a log
  (`./app macro > run.log 2>&1`), and read `run.log`. The tool's own output
  file stays empty when you redirect inside the command.
- Serial vs multithreaded: see `run.cli`. Try Serial first. MT multiplies
  memory use; the harness kills background jobs under memory pressure, and you
  must not restart one on your own after that.
- Smoke test: use `run.smoke` (low energy, few events), never the production macro.

## 4. Verify a run

Do not trust the exit code alone. Check all of:

1. Exit code 0.
2. Every `run.successMarkers` line is in the log.
3. No `run.failureMarkers`. Geant4 prints `WWWW ... G4Exception` for warnings
   and `EEEE ... G4Exception` for errors/fatal. Read the warnings once; only
   `run.benignWarnings` may be ignored.
4. Each file in `run.outputs` exists, is non-empty, and the numbers are
   plausible (open the head of each; don't just `ls`).

Output files are only written when a macro issues `/run/dumpDataAndReset`
(or, if it never does, once automatically at process exit under the
`EndOfRun_` prefix) -- several `/run/beamOn` calls between dumps just
accumulate into that one write. Reusing a prefix within the same process is
a fatal error rather than a silent `*_bis.csv` rename; give each
`/run/dumpDataAndReset` call in a macro a distinct prefix if you want
separate output per segment.

## 5. Unit tests

Two build directories, on purpose:

- `build.runBuildDir` (RelWithDebInfo) defines `NDEBUG`, which compiles
  `assert()` away. Plain-assert tests built there pass vacuously.
- `build.testBuildDir` (Debug) keeps `assert()`. Run tests only from here.

```
ctest --test-dir <testBuildDir> -N                         # list tests
cmake --build <testBuildDir> --target <each test target>   # build them first
ctest --test-dir <testBuildDir> --output-on-failure
```

- ctest reports `Not Run` for a test whose executable is not built yet. Build
  all listed targets first; success means every test `Passed`, none `Not Run`.
- MSVC Debug CRT: a failing `assert()` opens a blocking dialog, so a failing
  test hangs instead of exiting. New test executables should start `main()` with
  ```cpp
  #ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  #endif
  ```
  (`#include <crtdbg.h>`). Run a test exe by hand as `timeout 15 ./XTest.exe`.
- Test-first: the red step must be an assertion failure. A link error
  (`LNK2019 unresolved external`) means the test target lacks a source the
  code under test calls (e.g. a logger); add that `.cc` to the target.
- Tests link `${Geant4_LIBRARIES}` but must not need a Geant4 kernel, run
  manager or SD. Keep the logic under test as plain functions taking strings,
  numbers and streams; code that needs live Geant4 objects is checked by the
  smoke run in section 4 instead.

## 6. Housekeeping

- Build dirs are git-ignored; do not commit smoke output.
- A post-edit hook may re-run configure in `build.compileCommands.dir` to
  refresh `compile_commands.json` after each C++ edit; clangd diagnostics can
  lag one edit behind (stale "undeclared identifier" right after a header edit).
