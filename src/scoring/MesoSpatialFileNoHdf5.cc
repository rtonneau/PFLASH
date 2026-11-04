/// \file MesoSpatialFileNoHdf5.cc
/// \brief No-HDF5 build of MesoSpatialFile: same interface, no spatial file.
/// Standard library only.
///
/// Portable and project-agnostic: standard library only, no HDF5, no Geant4, no
/// dependency on other dnachem-min classes (adjust the include path prefix on
/// copy). Compiled instead of MesoSpatialFile.cc when HDF5 is not available
/// (CMake option DNACHEM_WITH_HDF5). FileName/StagingDir/StagedPath are identical
/// to the HDF5 version; AppendEvent always fails with an explanatory message and
/// MoveStaged finds nothing to move.

#include "scoring/MesoSpatialFile.hh"

bool MesoSpatialFile::Available()
{
  return false;
}

std::string MesoSpatialFile::FileName()
{
  return "SpeciesMesoSpatial.h5";
}

std::string MesoSpatialFile::StagingDir(const std::string& outputDir)
{
  if (outputDir.empty()) return ".pending_meso_spatial";
  return outputDir + "/.pending_meso_spatial";
}

std::string MesoSpatialFile::StagedPath(const std::string& outputDir)
{
  return StagingDir(outputDir) + "/" + FileName();
}

bool MesoSpatialFile::AppendEvent(const std::string& /*path*/,
                                  const std::vector<std::string>& /*species*/,
                                  const EventData& /*event*/, std::string& err)
{
  err = "built without HDF5 (rebuild with -DDNACHEM_WITH_HDF5=ON)";
  return false;
}

bool MesoSpatialFile::MoveStaged(const std::string& /*outputDir*/, const std::string& /*target*/,
                                 bool& moved, std::string& err)
{
  (void)err;
  moved = false;
  return true;
}
