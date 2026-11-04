/// \file MesoSpatialFile.cc
/// \brief Write and publish per-event Mesoscopic stage snapshots.
/// Standard library and HDF5 C++ only.
/// AppendEvent serializes HDF5 access; MoveStaged requires writers to be idle.
///
/// Portable: standard library and HDF5 C++ only.

#include "scoring/MesoSpatialFile.hh"

#include <H5Cpp.h>

#include <algorithm>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace
{
  constexpr int kChunkRows = 4096;
  constexpr int kDeflateLevel = 4;
  constexpr int kCoordinatesPerCell = 3;

  const char* const kUnits =
    "position_nm: nm (cell centre, world frame); cellSize_nm: nm (cell side); "
    "time_ns: ns; counts: molecules per cell";

  std::mutex& Hdf5Mutex()
  {
    static std::mutex mutex;
    return mutex;
  }

  void WriteStringAttr(H5::H5Object& obj, const char* name, const std::string& value)
  {
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    H5::DataSpace scalar(H5S_SCALAR);
    H5::Attribute attr = obj.createAttribute(name, type, scalar);
    const char* ptr = value.c_str();
    attr.write(type, &ptr);
  }

  void WriteSpeciesAttr(H5::H5Object& obj, const std::vector<std::string>& species)
  {
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    hsize_t dim = species.size();
    H5::DataSpace space(1, &dim);
    H5::Attribute attr = obj.createAttribute("species", type, space);
    std::vector<const char*> ptrs;
    for (const auto& speciesCount : species) ptrs.push_back(speciesCount.c_str());
    if (dim > 0) attr.write(type, ptrs.data());
  }

  std::vector<std::string> ReadSpeciesAttr(H5::H5Object& obj)
  {
    std::vector<std::string> result;
    H5::Attribute attr = obj.openAttribute("species");
    H5::DataSpace space = attr.getSpace();
    hsize_t dim = 0;
    if (space.getSimpleExtentNdims() != 1) throw H5::Exception("species", "attribute is not 1-D");
    space.getSimpleExtentDims(&dim);
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    std::vector<char*> ptrs(dim, nullptr);
    if (dim > 0) attr.read(type, ptrs.data());
    for (char* p : ptrs) {
      result.emplace_back(p ? p : "");
      if (p) H5free_memory(p);
    }
    return result;
  }

  void WriteDataset(H5::Group& group, const char* name, const H5::PredType& type,
                    const void* data, hsize_t rows, hsize_t cols)
  {
    const hsize_t dims[2] = {rows, cols};
    H5::DataSpace space(2, dims);
    H5::DSetCreatPropList props;
    // H5Pset_deflate marks the filter optional; H5Z_pipeline skips an
    // unavailable optional filter on write. Avoid chunk-storage overhead when
    // this HDF5 build cannot compress (e.g. vcpkg without its zlib feature).
    // Chunk dims must be non-zero, so empty datasets stay contiguous too.
    if (rows > 0 && cols > 0 && H5Zfilter_avail(H5Z_FILTER_DEFLATE) > 0) {
      const hsize_t chunk[2] = {std::min<hsize_t>(rows, kChunkRows), cols};
      props.setChunk(2, chunk);
      props.setDeflate(kDeflateLevel);
    }
    H5::DataSet set = group.createDataSet(name, type, space, props);
    if (rows > 0 && cols > 0) set.write(data, type);
  }

  void WriteTimeAndSize(H5::Group& group, double time_ns, double cellSize_nm)
  {
    H5::DataSpace scalar(H5S_SCALAR);
    group.createAttribute("time_ns", H5::PredType::NATIVE_DOUBLE, scalar)
      .write(H5::PredType::NATIVE_DOUBLE, &time_ns);
    group.createAttribute("cellSize_nm", H5::PredType::NATIVE_DOUBLE, scalar)
      .write(H5::PredType::NATIVE_DOUBLE, &cellSize_nm);
  }

  void WriteRootAttributes(H5::H5File& h5, const std::vector<std::string>& species)
  {
    H5::Group root = h5.openGroup("/");
    WriteSpeciesAttr(root, species);
    const int version = MesoSpatialFile::kFormatVersion;
    H5::DataSpace scalar(H5S_SCALAR);
    root.createAttribute("formatVersion", H5::PredType::NATIVE_INT, scalar)
      .write(H5::PredType::NATIVE_INT, &version);
    WriteStringAttr(root, "units", kUnits);
    WriteStringAttr(root, "formatDoc", MesoSpatialFile::kFormatDoc);
  }

  void WriteSnapshotRecords(H5::Group& eventGroup, const MesoSpatialFile::EventData& event,
                            const hsize_t& speciesCount, std::vector<std::string>& firstGroup)
  {
    for (std::size_t k = 0; k < event.records.size(); ++k) {
      const MesoSpatialFile::Record& record = event.records[k];
      const MesoSpatialFile::Snapshot& snapshot = event.snapshots[record.snapshot];
      const std::string name = "snapshot" + std::to_string(k);
      H5::Group group = eventGroup.createGroup(name);
      WriteTimeAndSize(group, record.time_ns, snapshot.cellSize_nm);
      if (firstGroup[record.snapshot].empty()) {
        const hsize_t cellCount = snapshot.position_nm.size() / kCoordinatesPerCell;
        WriteDataset(group, "position_nm", H5::PredType::NATIVE_DOUBLE, snapshot.position_nm.data(),
                     cellCount, kCoordinatesPerCell);
        WriteDataset(group, "counts", H5::PredType::NATIVE_UINT32, snapshot.counts.data(), cellCount, speciesCount);
        firstGroup[record.snapshot] = name;
      }
      else {
        const std::string& sourceSnapshotGroup = firstGroup[record.snapshot];
        for (const char* datasetName : {"position_nm", "counts"}) {
          if (H5Lcreate_hard(eventGroup.getId(), (sourceSnapshotGroup + "/" + datasetName).c_str(), group.getId(), datasetName,
                             H5P_DEFAULT, H5P_DEFAULT) < 0) {
            throw H5::Exception("AppendEvent", "H5Lcreate_hard failed");
          }
        }
      }
    }
  }

  bool Validate(const std::vector<std::string>& species, const MesoSpatialFile::EventData& event,
                std::string& err)
  {
    const std::size_t speciesCount = species.size();
    for (std::size_t i = 0; i < event.snapshots.size(); ++i) {
      const auto& snapshot = event.snapshots[i];
      if (snapshot.position_nm.size() % kCoordinatesPerCell != 0) {
        err = "Snapshot " + std::to_string(i) + ": position_nm size is not a multiple of 3";
        return false;
      }
      const std::size_t cellCount = snapshot.position_nm.size() / kCoordinatesPerCell;
      if (snapshot.counts.size() != cellCount * speciesCount) {
        err = "Snapshot " + std::to_string(i) + ": counts size is not cells * species";
        return false;
      }
    }
    for (std::size_t k = 0; k < event.records.size(); ++k) {
      if (event.records[k].snapshot >= event.snapshots.size()) {
        err = "Record " + std::to_string(k) + ": snapshot index out of range";
        return false;
      }
    }
    return true;
  }
}

bool MesoSpatialFile::Available()
{
  return true;
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

bool MesoSpatialFile::AppendEvent(const std::string& path, const std::vector<std::string>& species,
                                  const EventData& event, std::string& err)
{
  if (!Validate(species, event, err)) return false;

  std::lock_guard<std::mutex> lock(Hdf5Mutex());
  try {
    H5::Exception::dontPrint();

    std::error_code ec;
    const fs::path file(path);
    if (file.has_parent_path()) {
      fs::create_directories(file.parent_path(), ec);
      if (ec) {
        err = "Cannot create folder '" + file.parent_path().string() + "': " + ec.message();
        return false;
      }
    }

    const bool exists = fs::exists(file, ec);
    H5::H5File h5 = exists ? H5::H5File(path, H5F_ACC_RDWR) : H5::H5File(path, H5F_ACC_EXCL);

    const std::string runName = "/run" + std::to_string(event.runId);
    const std::string eventName = runName + "/event" + std::to_string(event.eventId);

    if (!exists) {
      WriteRootAttributes(h5, species);
    }
    else {
      H5::Group root = h5.openGroup("/");
      if (!root.attrExists("species") || ReadSpeciesAttr(root) != species) {
        err = "File '" + path + "' has a different species list";
        return false;
      }
      if (H5Lexists(h5.getId(), runName.c_str(), H5P_DEFAULT) > 0 &&
          H5Lexists(h5.getId(), eventName.c_str(), H5P_DEFAULT) > 0) {
        err = "File '" + path + "' already has " + eventName;
        return false;
      }
    }

    H5::Group runGroup = (H5Lexists(h5.getId(), runName.c_str(), H5P_DEFAULT) > 0)
                           ? h5.openGroup(runName)
                           : h5.createGroup(runName);
    H5::Group eventGroup = runGroup.createGroup("event" + std::to_string(event.eventId));

    const hsize_t speciesCount = species.size();
    std::vector<std::string> firstGroup(event.snapshots.size());
    WriteSnapshotRecords(eventGroup, event, speciesCount, firstGroup);
    h5.close();
    return true;
  }
  catch (const H5::Exception& e) {
    err = "HDF5 error on '" + path + "': " + e.getDetailMsg();
    return false;
  }
}

bool MesoSpatialFile::MoveStaged(const std::string& outputDir, const std::string& target,
                                 bool& moved, std::string& err)
{
  moved = false;
  const fs::path source = StagedPath(outputDir);
  std::error_code ec;
  if (!fs::is_regular_file(source, ec)) return true;

  const fs::path dest(target);
  if (dest.has_parent_path()) fs::create_directories(dest.parent_path(), ec);
  if (!ec) fs::remove(dest, ec);
  if (!ec) fs::rename(source, dest, ec);
  if (ec) {
    err = "Cannot move '" + source.string() + "' to '" + target + "': " + ec.message();
    return false;
  }
  moved = true;
  return true;
}
