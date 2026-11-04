# Prebuilt release (Windows x64)

Each GitHub Release carries `dnachem-min-win64.zip`, so that someone with a
Geant4 install but no build setup can run a macro from the explorer's Macro
builder. The Macro builder links to
`https://github.com/rtonneau/dnachem-min/releases/latest/download/dnachem-min-win64.zip`;
the asset name is fixed, so that URL always serves the newest release.

## What the zip holds

Defined by the `install()`/CPack section of `CMakeLists.txt`:

| File | Source |
|---|---|
| `sim.exe` | the run build (`build/`, RelWithDebInfo) |
| `hdf5.dll`, `hdf5_cpp.dll` | the vcpkg HDF5 imported targets; they import only the MSVC runtime. The release zip is built with HDF5 on (`DNACHEM_WITH_HDF5=ON`); a build without HDF5 omits these DLLs |
| `macro/*.in`, `macro/*.mac` | `macro/` |
| `README.txt`, `check-env.ps1` | `packaging/` |
| `VERSION.txt` | written at packaging time: `git describe` and the Geant4 version |

Not shipped: the Geant4 DLLs, Qt, the Geant4 datasets and the MSVC runtime.
`sim.exe` imports the `G4*.dll` by name, so the user's Geant4 must be the same
version and build options (MSVC x64, shared, multithreaded, Qt and OpenGL,
which `sim.exe` imports through `G4interfaces.dll` and `G4OpenGL.dll`).
`packaging/README.txt` states this for users and `check-env.ps1` checks it.

## Make a release

Built locally, not on CI (CI would need a Geant4 build). In the MSVC x64
environment (`.claude/geant4-instructions.md` section 1), from the repository
root, on a clean, tagged commit so `VERSION.txt` carries the tag:

```powershell
git tag v1.1.0
cmake --build build --target sim
cd build; cpack -G ZIP; cd ..        # -> build/dnachem-min-win64.zip
```

Smoke-test the zip outside the repository: unzip it to an empty folder, run
`check-env.ps1`, then `.\sim.exe example_irt.in` (about a minute; it must end
with two `dumped and reset` lines and exit 0).

Publish:

```powershell
git push origin v1.1.0
gh release create v1.1.0 build/dnachem-min-win64.zip --title "dnachem-min v1.1.0" --notes "..."
```

Keep the asset name `dnachem-min-win64.zip` (the explorer link depends on it).
A macro from the Macro builder can use commands an older `sim.exe` lacks, so
publish a new release when macro commands change.
