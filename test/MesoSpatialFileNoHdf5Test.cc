/// \file MesoSpatialFileNoHdf5Test.cc
/// Dependencies: Standard library and project code; no HDF5, no Geant4 dependency.
/// Thread safety: Sequential test process.
/// \brief Plain-assert unit tests for the no-HDF5 stub of MesoSpatialFile (no
/// test framework, no Geant4 runtime).

#include "scoring/MesoSpatialFile.hh"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

// assert() is compiled away under NDEBUG, so this test uses its own check.
#define CHECK(cond)                                                                     \
  do {                                                                                  \
    if (!(cond)) {                                                                      \
      std::cerr << "CHECK failed: " #cond " at line " << __LINE__ << std::endl;         \
      std::abort();                                                                     \
    }                                                                                   \
  } while (0)

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  CHECK(!MesoSpatialFile::Available());
  CHECK(MesoSpatialFile::FileName() == "SpeciesMesoSpatial.h5");
  CHECK(MesoSpatialFile::StagingDir("") == ".pending_meso_spatial");
  CHECK(MesoSpatialFile::StagingDir("out") == "out/.pending_meso_spatial");
  CHECK(MesoSpatialFile::StagedPath("out") == "out/.pending_meso_spatial/SpeciesMesoSpatial.h5");

  MesoSpatialFile::EventData event;
  std::string err;
  CHECK(!MesoSpatialFile::AppendEvent("unused.h5", {"H2O"}, event, err));
  CHECK(!err.empty());

  bool moved = true;
  std::string moveErr;
  CHECK(MesoSpatialFile::MoveStaged("out", "target.h5", moved, moveErr));
  CHECK(!moved);

  std::cout << "MesoSpatialFileNoHdf5Test passed" << std::endl;
  return 0;
}
