---
bump: patch
floor: patch
---
- Linux: building with HDF5 (`DNACHEM_WITH_HDF5=ON`, or `AUTO` when HDF5 is found) no longer fails at configure with `No target "hdf5::hdf5-shared"`; distro HDF5 found by CMake's FindHDF5 is now supported.
- README: corrected Linux build, run and test commands (separate Debug tree for the tests), package list and a WSL note.
