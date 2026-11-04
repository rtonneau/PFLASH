/// \file Hdf5TableFileNoHdf5.cc
/// \brief No-HDF5 build of Hdf5TableFile: same interface, no file written.
/// Standard library and OutputTable only. Compiled instead of Hdf5TableFile.cc
/// when HDF5 is not available (CMake option DNACHEM_WITH_HDF5).

#include "scoring/Hdf5TableFile.hh"

bool Hdf5TableFile::Available()
{
  return false;
}

bool Hdf5TableFile::Write(const std::string& /*path*/, const std::vector<OutputTable>& /*tables*/,
                          std::string& err)
{
  err = "built without HDF5 (rebuild with -DDNACHEM_WITH_HDF5=ON)";
  return false;
}
