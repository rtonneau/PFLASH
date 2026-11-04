/// \file Hdf5TableFileNoHdf5Test.cc
/// Dependencies: Standard library and project code; no HDF5, no Geant4.
/// \brief Plain-check unit tests for the no-HDF5 stub of Hdf5TableFile.

#include "scoring/Hdf5TableFile.hh"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::cerr << "CHECK failed: " #cond " at line " << __LINE__ << std::endl;  \
      std::abort();                                                              \
    }                                                                            \
  } while (0)

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
#endif
  CHECK(!Hdf5TableFile::Available());
  std::vector<OutputTable> tables;
  tables.emplace_back("T", "d");
  std::string err;
  CHECK(!Hdf5TableFile::Write("unused.h5", tables, err));
  CHECK(err.find("-DDNACHEM_WITH_HDF5=ON") != std::string::npos);
  std::cout << "Hdf5TableFileNoHdf5Test passed\n";
  return 0;
}
