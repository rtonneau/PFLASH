dnachem-min: prebuilt simulation for Linux x86-64
=================================================

Minimal Geant4-DNA simulation of water radiolysis by electron beams.
Source, documentation and the Macro builder:
  https://github.com/rtonneau/dnachem-min
  https://rtonneau.github.io/dnachem-min/explorer/#builder

VERSION.txt gives the dnachem-min version and the Geant4 version it was
built against.


What is in this folder
----------------------
  sim                the simulation
  macro/             example macros (*.in)
  VERSION.txt        version information


What you must provide (NOT shipped)
-----------------------------------
sim loads the Geant4 libraries of YOUR Geant4 install. That install must
match the one sim was built against, or sim will not start or will crash:

  * Geant4 version given in VERSION.txt (exactly)
  * built with GCC, shared libraries, GEANT4_BUILD_MULTITHREADED=ON
  * its datasets installed
  * Geant4's environment loaded in every shell that runs sim:
      source <geant4-install>/bin/geant4.sh
  * the HDF5 C++ shared library from your distribution
      Ubuntu/Debian: sudo apt install libhdf5-cpp-103 (or libhdf5-dev)
  * built on Ubuntu 24.04 (glibc 2.39): older distributions may be too old

Check what is missing with:  ldd ./sim | grep "not found"


Running
-------
1. Put your macro (for example one saved from the Macro builder) into the
   macro/ folder, or next to sim.
2. From this folder:

     ./sim my_macro.in
     ./sim my_macro.in --threads 4       (multithreaded)
     ./sim my_macro.in --dir /data/a     (write output there)

   sim looks for the macro as given, then next to sim, then in macro/.
3. Output goes to results/ next to sim unless --dir or /run/outputDir is
   given. Every dump has a Manifest.json describing its files.

Quick test: ./sim example_irt.in  (10 keV electrons, two small sub-runs,
about a minute).
