# Plan: make the HDF5 build work on Linux (FindHDF5) + fix the Linux README

Date: 2026-11-04. Status: awaiting approval.

## Problem (observed in WSL, Ubuntu 24.04, GCC 13.3, Geant4 11.4.3, `main` @ d41c924)

`-DDNACHEM_WITH_HDF5=ON` (and `AUTO` whenever HDF5 is found) fails at configure:

```
Error evaluating generator expression: $<TARGET_FILE:hdf5::hdf5-shared>
No target "hdf5::hdf5-shared"
Target "sim" links to: hdf5::hdf5-shared but the target was not found.
```

`hdf5::hdf5-shared` / `hdf5::hdf5_cpp-shared` exist only when HDF5's own CMake
package config is used (vcpkg on Windows). Distro HDF5 (Ubuntu `libhdf5-dev`
1.10.10) is found by CMake's FindHDF5 module, which defines `hdf5::hdf5` /
`hdf5::hdf5_cpp` (CMake >= 3.19) and always sets `HDF5_LIBRARIES`,
`HDF5_INCLUDE_DIRS`, `HDF5_DEFINITIONS`. The project allows CMake 3.16.

Hardcoded uses in `CMakeLists.txt`: line 66 (`sim`), line 174
(`MesoSpatialFileTest`), line 211 (`install(FILES $<TARGET_FILE:...>)` for the
Windows release zip).

## Change 1: `CMakeLists.txt`, one HDF5 link target

Right after the `DNACHEM_HAS_HDF5` decision (around line 36), when it is TRUE,
define an INTERFACE target `dnachem_hdf5` from the first form that exists:

1. `hdf5::hdf5-shared` + `hdf5::hdf5_cpp-shared` (HDF5 config package, vcpkg):
   link both, set `_dnachemHdf5Dlls` to their `$<TARGET_FILE:...>` for install.
2. `hdf5::hdf5` + `hdf5::hdf5_cpp` (FindHDF5 imported targets, CMake >= 3.19).
3. Fallback: `${HDF5_LIBRARIES}` (C++ and C, in that order, from FindHDF5),
   `${HDF5_INCLUDE_DIRS}`, `${HDF5_DEFINITIONS}`.

If none of these is available although HDF5_FOUND: with `ON` a `FATAL_ERROR`
naming what was found; with `AUTO` a STATUS message and `DNACHEM_HAS_HDF5 FALSE`.

Then `sim` (line 66) and `MesoSpatialFileTest` (line 174) link `dnachem_hdf5`.

## Change 2: `CMakeLists.txt`, packaging

Line 210: install the HDF5 DLLs only `if(WIN32 AND _dnachemHdf5Dlls)`. On Linux
the shared libraries come from the system package and are not bundled; the
release zip (`docs/release.md`) is Windows-only and keeps its behaviour.

## Change 3: README "Linux (not yet verified)"

- Fix the snippet: currently `cd build && ./sim beam.in` then
  `ctest --test-dir build` (looks for `build/build`). Use a run tree and a
  separate Debug tree for tests, mirroring Windows:
  ```bash
  source /path/to/geant4-install/bin/geant4.sh
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo [-DDNACHEM_WITH_HDF5=OFF|ON]
  cmake --build build -j
  (cd build && ./sim beam.in)
  cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-debug -j && ctest --test-dir build-debug
  ```
  Note why: tests use plain `assert`, compiled out by `NDEBUG` in
  RelWithDebInfo/Release.
- Packages: `sudo apt install build-essential cmake ninja-build libexpat1-dev libxerces-c-dev [libhdf5-dev]`.
- WSL note: by default WSL appends the Windows PATH, so CMake can pick up the
  Windows vcpkg HDF5 (`ssize_t` conflict in `H5public.h`); set
  `[interop] appendWindowsPath=false` in `/etc/wsl.conf`.
- Heading: change "not yet verified" to "verified on Ubuntu 24.04 (WSL 2),
  GCC 13.3, Geant4 11.4.3" only after Verification below passes; also update
  README line 55 (gcc "not yet verified").
- Geant4 11.4.3 is not the verified 11.4.1: state the Linux test version, do
  not change the project-wide "verified with 11.4.1" claim.

## Out of scope

- The `G4String::toLower()` deprecation warning (`DnaLogger.cc:71`); separate.
- Making Geant4's own HDF5 dependency optional (already documented in README).

## Verification

Windows (must not regress):
- `build/` RelWithDebInfo with vcpkg HDF5 configures and builds; `cpack` zip
  still contains the two HDF5 DLLs.
- `build-ninja/` Debug: full ctest passes (with vcpkg `debug\bin` on PATH, see
  memory note on the MesoSpatialFileTest hang).

Linux (WSL, `~/dnachem-min`, branch pulled there):
- `DNACHEM_WITH_HDF5=ON`, `AUTO`, `OFF`: configure + build.
- Debug + HDF5: ctest passes including `MesoSpatialFileTest`.
- Smoke: 10 keV e-, `/run/beamOn 2`, `/scheduler/endTime 1 ms`,
  `/chem/meso/spatialOutput true` → `SpeciesMesoSpatial.h5` present, manifest
  `"hdf5": true`.

## Delivery

Branch `fix/linux-hdf5-targets` from `main`, one commit for CMake, one for
README, PR to `main`. CHANGELOG entry (patch).
