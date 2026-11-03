dnachem-min: prebuilt simulation for Windows x64
=================================================

Minimal Geant4-DNA simulation of water radiolysis by electron beams.
Source, documentation and the Macro builder:
  https://github.com/rtonneau/dnachem-min
  https://rtonneau.github.io/dnachem-min/explorer/#builder

VERSION.txt gives the dnachem-min version and the Geant4 version it was
built against.


What is in this folder
----------------------
  sim.exe            the simulation
  hdf5.dll           HDF5 C library (needed by sim.exe, shipped here)
  hdf5_cpp.dll       HDF5 C++ library (needed by sim.exe, shipped here)
  macro\             example macros (*.in)
  check-env.ps1      checks that your Geant4 environment can run sim.exe
  VERSION.txt        version information


What you must provide (NOT shipped)
-----------------------------------
sim.exe loads the Geant4 libraries (G4*.dll) of YOUR Geant4 install by name.
That install must match the one sim.exe was built against, or sim.exe will
not start or will crash:

  * Geant4 version given in VERSION.txt (exactly; e.g. 11.4.1)
  * built with MSVC, x64, shared libraries (GEANT4_BUILD_MULTITHREADED=ON)
  * with Qt and OpenGL (sim.exe imports G4interfaces.dll and G4OpenGL.dll)
  * its bin\ folder on PATH, and the Qt bin\ folder it was built with on PATH
  * its datasets installed (G4LEDATA, G4ENSDFSTATEDATA, ... set, or found
    through the install as usual)
  * the Microsoft Visual C++ 2015-2022+ redistributable (x64), recent:
    https://aka.ms/vs/17/release/vc_redist.x64.exe

Check it from this folder in PowerShell:

  powershell -ExecutionPolicy Bypass -File .\check-env.ps1


Running
-------
1. Put your macro (for example one saved from the Macro builder) into the
   macro\ folder, or next to sim.exe.
2. From this folder:

     .\sim.exe my_macro.in
     .\sim.exe my_macro.in --threads 4      (multithreaded)
     .\sim.exe my_macro.in --dir D:\runs\a  (write output there)

   sim.exe looks for the macro as given, then next to sim.exe, then in
   macro\.
3. Output goes to results\ next to sim.exe unless --dir or /run/outputDir is
   given. Every dump has a Manifest.json describing its files.

Quick test: .\sim.exe example_irt.in  (10 keV electrons, two small sub-runs,
about a minute).

If sim.exe exits at once without output (exit code -1073741515 /
0xC0000135), a DLL is missing: run check-env.ps1.
