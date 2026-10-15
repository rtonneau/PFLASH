# Stock scavenger example reference: e_aq G(t), 1 ps to 1 ns

Reference run of the unmodified Geant4 11.4.1 `examples/extended/medical/dna/scavenger`
example, for comparing against dnachem-min's `Farokhi2023` e_aq curve. Result:
`data/stock_scavenger_eaq.csv` (columns `time_s,G_eaq_per_100eV,SEM,n_events`; 31 log-spaced times,
10 per decade; e_aq G = 4.18 at 1 ps, 3.76 at 1 ns).

## Settings used

| Item | Value |
|---|---|
| Beam | 500 keV e-, `/gps/particle e-`, `/gps/energy 500 keV` |
| PrimaryKiller | `eLossMin 12.5 keV`; `eLossMax 1 MeV` (the example hardcodes 1 keV, which aborts every event, so it must be set; 1 MeV is dnachem-min's default) |
| Events | 250 (one `/run/beamOn 250`), 8 worker threads |
| O2 | 0 %: stock `Rtable_O2.txt` copied to `Rtable_O2_0pct.txt` with `scavenger: O2 0.0` (the three O2 reactions are then dropped with `parchem01`/`parchem03` warnings, expected) |
| Solvation subtype | `Meesungnoen2002` (the stock `beam.in` choice; dnachem-min's Farokhi macros use `Ritchie1994`, so this is NOT matched) |
| Physics | stock `G4EmDNAPhysics_option2` + stock `EmDNAChemistry` (IRT, no mesoscopic stage) |
| Seed | `G4Random::setTheSeed(12345)` via the example's second CLI argument, same value as dnachem-min's default seed (`kDefaultSeed`, `sim.cc`) |
| End time | `/scheduler/endTime 1.01 ns`; recording times added explicitly with `/scorer/species/addTimeToRecord` at 10^(k/10) ps, k = 0..30 |

## Build commands

Example sources are not modified; the build is in the scratch dir
`.scratch/tests/2026-10-13__eaq-g-value-gap-vs-farokhi-at-1-ps/stock-scavenger/build` (git-ignored).
Run from a cmd shell with the MSVC environment (Qt5 DLLs/prefix needed because the Geant4 install links Qt):

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=E:\miniconda3\envs\qt5\Library\bin;%PATH%"
cmake -S %DEV_DIR%\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1\examples\extended\medical\dna\scavenger ^
      -B <scratch>\stock-scavenger\build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_GEANT4_UIVIS=OFF ^
      -DGeant4_DIR=%G4_ROOT%/lib/cmake/Geant4 -DCMAKE_PREFIX_PATH=E:/miniconda3/envs/qt5/Library ^
      -DCMAKE_TOOLCHAIN_FILE=%DEV_DIR%/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build <scratch>\stock-scavenger\build --target scavenger
```

## Run

```bat
cd <scratch>\stock-scavenger\build
.\scavenger.exe stock_scavenger_eaq.in 12345
```

Took about 108 s wall; 250 of 250 events recorded, total energy deposit 3.806 MeV.
The example writes `stock_scavenger.root` (ntuple `species`: `time` in ns, `sumG`, `sumG2`, `nEvent`, `speciesName`).

Macro `stock_scavenger_eaq.in` (the 31 `addTimeToRecord` lines abbreviated):

```
# Stock Geant4 11.4.1 scavenger example, 500 keV e-, eLossMin 12.5 keV, 0 % O2.
/run/numberOfThreads 8
/process/dna/e-SolvationSubType Meesungnoen2002
/chem/reactionTable/name Rtable_O2_0pct.txt
/run/initialize
/primaryKiller/eLossMin 12.5 keV
/primaryKiller/eLossMax 1 MeV
/gps/particle e-
/gps/energy 500 keV
/scheduler/endTime 1.01 ns
/scorer/species/addTimeToRecord 1 ps
... (k = 1..29: 10^(k/10) ps)
/scorer/species/addTimeToRecord 1000 ps
/scorer/species/setRootFileName stock_scavenger.root
/run/verbose 1
/scheduler/verbose 0
/run/beamOn 250
```

## Conversion to CSV

For the `e_aq^-1` rows, same estimator as `plot_gvalues.py` (Bessel-corrected):
`mean = sumG/n`, `var = (sumG2 - n*mean^2)/(n-1)`, `SEM = sqrt(var/n)`, `time_s = time_ns * 1e-9`.
Read with `uproot` (installed with `pip install --target` into the scratch dir; script at
`.scratch/tests/2026-10-13__eaq-g-value-gap-vs-farokhi-at-1-ps/root_to_csv.py`).
