/// \file MesoSpatialFile.cc
/// \brief Implementation of the MesoSpatialFile HDF5 writer
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
    for (const auto& s : species) ptrs.push_back(s.c_str());
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
    // gzip only when this HDF5 build has the deflate filter (vcpkg's hdf5
    // without its zlib feature lacks it, and setDeflate would then be a silent
    // no-op whose padded chunks make the file larger than contiguous storage).
    // Chunk dims must be non-zero, so empty datasets stay contiguous too.
    if (rows > 0 && cols > 0 && H5Zfilter_avail(H5Z_FILTER_DEFLATE) > 0) {
      const hsize_t chunk[2] = {std::min<hsize_t>(rows, 4096), cols};
      props.setChunk(2, chunk);
      props.setDeflate(4);
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

  bool Validate(const std::vector<std::string>& species, const MesoSpatialFile::EventData& event,
                std::string& err)
  {
    const std::size_t s = species.size();
    for (std::size_t i = 0; i < event.snapshots.size(); ++i) {
      const auto& snap = event.snapshots[i];
      if (snap.position_nm.size() % 3 != 0) {
        err = "Snapshot " + std::to_string(i) + ": position_nm size is not a multiple of 3";
        return false;
      }
      const std::size_t n = snap.position_nm.size() / 3;
      if (snap.counts.size() != n * s) {
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
      H5::Group root = h5.openGroup("/");
      WriteSpeciesAttr(root, species);
      const int version = kFormatVersion;
      H5::DataSpace scalar(H5S_SCALAR);
      root.createAttribute("formatVersion", H5::PredType::NATIVE_INT, scalar)
        .write(H5::PredType::NATIVE_INT, &version);
      WriteStringAttr(root, "units", kUnits);
      WriteStringAttr(root, "formatDoc", kFormatDoc);
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

    const hsize_t s = species.size();
    std::vector<std::string> firstGroup(event.snapshots.size());
    for (std::size_t k = 0; k < event.records.size(); ++k) {
      const Record& rec = event.records[k];
      const Snapshot& snap = event.snapshots[rec.snapshot];
      const std::string name = "snapshot" + std::to_string(k);
      H5::Group group = eventGroup.createGroup(name);
      WriteTimeAndSize(group, rec.time_ns, snap.cellSize_nm);
      if (firstGroup[rec.snapshot].empty()) {
        const hsize_t n = snap.position_nm.size() / 3;
        WriteDataset(group, "position_nm", H5::PredType::NATIVE_DOUBLE, snap.position_nm.data(),
                     n, 3);
        WriteDataset(group, "counts", H5::PredType::NATIVE_UINT32, snap.counts.data(), n, s);
        firstGroup[rec.snapshot] = name;
      }
      else {
        const std::string& src = firstGroup[rec.snapshot];
        for (const char* ds : {"position_nm", "counts"}) {
          if (H5Lcreate_hard(eventGroup.getId(), (src + "/" + ds).c_str(), group.getId(), ds,
                             H5P_DEFAULT, H5P_DEFAULT) < 0) {
            throw H5::Exception("AppendEvent", "H5Lcreate_hard failed");
          }
        }
      }
    }
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
