# Installation

Build instructions for Windows (MSVC) and Linux (GCC). Back to the [README](../README.md).

## Requirements

- Geant4 with DNA models (verified with 11.4.3; older versions are not verified)
- CMake 3.16+
- A C++20 compiler (verified with MSVC on Windows; GCC 13.3 on Linux, Ubuntu 24.04 with Geant4 11.4.3; clang not verified)
- HDF5 with C++ support, optional: needed only for the `h5` output format


There are two build trees, on both platforms, on purpose:

- a **run tree** (RelWithDebInfo) to run `sim`;
- a **test tree** (Debug) to run the unit tests with CTest. The tests use plain
  `assert`, which `NDEBUG` compiles out in RelWithDebInfo and Release, so they
  would pass there without checking anything.

The build copies `macro/` next to the executable, so `sim` finds the example
macros by filename.

## HDF5 option (both platforms)

`DNACHEM_WITH_HDF5` is `AUTO` by default: HDF5 is used if CMake finds it. `ON`
requires it (configure fails otherwise); `OFF` builds without it. The configure
log says which form it used (`-- HDF5: using ...`). Without HDF5, choosing the
`h5` format for any product (`/output/<product>/format h5`, or in
`/output/defaultFormat`) is a fatal error and `Manifest.json` has
`"hdf5": false`; everything else works, including `root`.
Note that `find_package(Geant4)` itself pulls in HDF5 when Geant4 was built
with HDF5 analysis support
(`GEANT4_USE_HDF5=ON`). dnachem-min's own code then no longer needs HDF5, but a
fully HDF5-free build also needs a Geant4 built without HDF5
(`GEANT4_USE_HDF5=OFF`, the Geant4 default).

## Windows (verified: Windows 11, MSVC, Geant4 11.4.3, vcpkg HDF5 1.14.6)

<details><summary>Windows steps</summary>

No build setup? A prebuilt `dnachem-min-win64.zip` (`sim.exe`, the HDF5 DLLs and
`macro/`) is attached to each [GitHub Release](https://github.com/rtonneau/dnachem-min/releases/latest).
It still needs your own Geant4 install, of the same version and build options;
see `packaging/README.txt` in the zip and [`docs/release.md`](release.md).

**1. Tools.** Install Visual Studio 2022 or later (Community is enough) with the
*Desktop development with C++* workload. It provides MSVC, CMake and Ninja
(*C++ CMake tools for Windows* component). Git is needed to clone the repository.

**2. Geant4.** Build and install Geant4 from source with MSVC x64, shared
libraries, multithreading and the datasets, for example (from the
*x64 Native Tools Command Prompt*, paths to adapt):

```bat
cmake -S geant4-v11.4.3 -B geant4-build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_INSTALL_PREFIX=C:/path/to/geant4-v11.4.3-install ^
      -DGEANT4_BUILD_MULTITHREADED=ON -DGEANT4_INSTALL_DATA=ON
cmake --build geant4-build
cmake --install geant4-build
```

Add Qt/OpenGL options (`-DGEANT4_USE_QT=ON`, ...) only if you want the GUI; a
Geant4 built with Qt then needs Qt's `bin\` on `PATH` when `sim.exe` runs.

**3. HDF5 (optional).** Install it with [vcpkg](https://github.com/microsoft/vcpkg)
including the C++ library:

```powershell
git clone https://github.com/microsoft/vcpkg C:/path/to/vcpkg
C:/path/to/vcpkg/bootstrap-vcpkg.bat
C:/path/to/vcpkg/vcpkg install "hdf5[cpp]:x64-windows"
```

Pass vcpkg's toolchain file at configure (below); it lets CMake find HDF5 and
copies `hdf5.dll` / `hdf5_cpp.dll` next to `sim.exe`. Without the toolchain file
`AUTO` simply builds without HDF5.

**4. Configure and build.** Compiling needs the MSVC x64 environment (otherwise
it fails on missing STL headers such as `cstddef`). Open the
*x64 Native Tools Command Prompt for VS*, or load `vcvars64.bat` in PowerShell:

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && <command>"
```

Then, from the repository root (`[-DDNACHEM_WITH_HDF5=OFF|ON]` is optional):

```powershell
# run tree
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
      -DGeant4_DIR=C:/path/to/geant4-v11.4.3-install/lib/cmake/Geant4 `
      -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --target sim

# test tree
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DGeant4_DIR=C:/path/to/geant4-v11.4.3-install/lib/cmake/Geant4 `
      -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-ninja
```

**5. Run.** From PowerShell (not Git Bash), in `build\`, with Geant4's `bin\`
(and Qt's, if Geant4 uses Qt) on `PATH`:

```powershell
$env:PATH = "C:/path/to/geant4-v11.4.3-install/bin;$env:PATH"
cd build
.\sim.exe example_irt.in
```

If `sim.exe` exits at once with code `0xC0000135`, a DLL was not found
(typically Geant4's or Qt's): add its directory to `PATH`.

**6. Test.** The Debug tests need vcpkg's *Debug* HDF5 DLLs first on `PATH`
(with the release DLLs first, `MesoSpatialFileTest` can hang):

```powershell
$env:PATH = "C:/path/to/vcpkg/installed/x64-windows/debug/bin;$env:PATH"
ctest --test-dir build-ninja --output-on-failure
```

Every test must report `Passed`; `Not Run` means its executable was not built.
The Node-based checks (explorer, macro builder, `CatalogExportCheck`) run only
when `node` is on `PATH`; `CatalogExportCheck` uses `build\sim.exe`, so build the
run tree first. More detail (background runs, verification checklist):
[`.claude/geant4-instructions.md`](../.claude/geant4-instructions.md).

</details>

## Linux (verified: Ubuntu 24.04 (WSL 2), GCC 13.3, Geant4 11.4.3, HDF5 1.10.10)

<details><summary>Linux and WSL steps</summary>

**1. Packages** (Ubuntu/Debian; `libhdf5-dev` only for HDF5 support, it
provides the C++ library too):

```bash
sudo apt install build-essential cmake ninja-build git \
                 libexpat1-dev libxerces-c-dev libhdf5-dev
```

`libexpat1-dev` and `libxerces-c-dev` are for the Geant4 build below (system
Expat, GDML); drop them if your Geant4 is configured without those options.

**2. Geant4.** Build and install it from source, for example (the verified
setup; Qt is not needed, `sim` runs in batch mode):

```bash
cmake -S geant4-v11.4.3 -B geant4-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/geant4/install \
      -DGEANT4_BUILD_MULTITHREADED=ON -DGEANT4_INSTALL_DATA=ON \
      -DGEANT4_USE_GDML=ON -DGEANT4_USE_SYSTEM_EXPAT=ON
cmake --build geant4-build -j
cmake --install geant4-build
```

Source `geant4.sh` in every shell that builds or runs `sim`: it puts Geant4 on
CMake's search path and on `LD_LIBRARY_PATH`, and sets the dataset variables
(`G4LEDATA`, ...).

```bash
source $HOME/geant4/install/bin/geant4.sh
```

**3. Configure and build**, from the repository root (`[-DDNACHEM_WITH_HDF5=OFF|ON]`
is optional; the distro HDF5 is found by CMake's FindHDF5 module):

```bash
# run tree
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# test tree
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```

**4. Run**, from the run tree:

```bash
cd build && ./sim example_irt.in
```

**5. Test:**

```bash
ctest --test-dir build-debug --output-on-failure
```

Every test must report `Passed`. As on Windows, the Node-based checks run only
when `node` is installed, and `CatalogExportCheck` uses `build/sim` (build the
run tree first, or set `-DCATALOG_EXPORT_SIM=<path>`).

**WSL.** By default WSL appends the Windows `PATH`, so CMake can pick up a Windows
HDF5 (e.g. vcpkg's), which fails to compile (`ssize_t` conflict in
`H5public.h`). Set `appendWindowsPath=false` under `[interop]` in
`/etc/wsl.conf` and restart WSL (`wsl --shutdown`). Clone the repository into the
Linux file system (e.g. `~/dnachem-min`), not under `/mnt/c`: builds there are
much slower.

</details>

