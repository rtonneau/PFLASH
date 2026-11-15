#include "SnapshotWriter.hh"

#include <H5Cpp.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace drs
{

namespace
{

constexpr hsize_t kChunkRows = 65536;
constexpr int kDeflateLevel = 4;
constexpr const char* kUnits = "position_nm: nm (world frame); time_ns: ns; species: index into /species";

H5::StrType Utf8String()
{
  H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
  type.setCset(H5T_CSET_UTF8);
  return type;
}

void WriteStringAttribute(H5::H5Object& object, const char* name, const std::string& value)
{
  const auto type = Utf8String();
  H5::DataSpace scalar(H5S_SCALAR);
  auto attribute = object.createAttribute(name, type, scalar);
  const char* pointer = value.c_str();
  attribute.write(type, &pointer);
}

std::string FrameName(std::size_t index)
{
  char name[32];
  std::snprintf(name, sizeof name, "frame%06zu", index);
  return name;
}

// Chunked, shuffled and gzip compressed when the filter exists; an empty
// dataset stays contiguous (a chunk needs a non-zero size). columns == 0 makes
// a one-dimensional dataset.
void WriteDataset(H5::Group& group, const char* name, const H5::PredType& type, const void* data, hsize_t rows,
                  hsize_t columns)
{
  hsize_t dims[2] = {rows, columns};
  const int rank = columns == 0 ? 1 : 2;
  H5::DataSpace space(rank, dims);
  H5::DSetCreatPropList properties;
  if (rows > 0 && H5Zfilter_avail(H5Z_FILTER_DEFLATE) > 0)
  {
    hsize_t chunk[2] = {std::min(rows, kChunkRows), columns};
    properties.setChunk(rank, chunk);
    properties.setShuffle();
    properties.setDeflate(kDeflateLevel);
  }
  auto set = group.createDataSet(name, type, space, properties);
  if (rows > 0) set.write(data, type);
}

}  // namespace

struct SnapshotWriter::Impl
{
  std::filesystem::path path;
  std::size_t speciesCount = 0;
  std::size_t frames = 0;
  std::unique_ptr<H5::H5File> file;
};

bool SnapshotWriter::Compressed()
{
  return H5Zfilter_avail(H5Z_FILTER_DEFLATE) > 0;
}

bool SnapshotWriter::Available()
{
  return true;
}

SnapshotWriter::SnapshotWriter(const std::filesystem::path& file, const std::vector<std::string>& species)
    : fImpl(std::make_unique<Impl>())
{
  if (species.size() > kMaxSnapshotSpecies)
    throw std::runtime_error("SnapshotWriter: " + std::to_string(species.size()) + " species do not fit a uint8 id");
  H5::Exception::dontPrint();
  fImpl->path = file;
  fImpl->speciesCount = species.size();
  try
  {
    fImpl->file = std::make_unique<H5::H5File>(file.string(), H5F_ACC_TRUNC);
    auto root = fImpl->file->openGroup("/");
    const int version = kSnapshotFormatVersion;
    root.createAttribute("formatVersion", H5::PredType::NATIVE_INT, H5::DataSpace(H5S_SCALAR))
        .write(H5::PredType::NATIVE_INT, &version);
    WriteStringAttribute(root, "units", kUnits);

    const auto type = Utf8String();
    hsize_t count = species.size();
    H5::DataSpace space(1, &count);
    auto set = root.createDataSet("species", type, space);
    std::vector<const char*> pointers;
    for (const auto& name : species) pointers.push_back(name.c_str());
    if (count > 0) set.write(pointers.data(), type);
    root.createGroup("frames");
  }
  catch (const H5::Exception& error)
  {
    throw std::runtime_error("SnapshotWriter: cannot create " + file.string() + ": " + error.getDetailMsg());
  }
}

SnapshotWriter::~SnapshotWriter()
{
  try
  {
    Close();
  }
  catch (...)
  {
  }
}

void SnapshotWriter::AddFrame(double timeNs, const std::vector<float>& positionsNm,
                              const std::vector<std::uint8_t>& species)
{
  if (!fImpl || !fImpl->file) throw std::runtime_error("SnapshotWriter: the file is closed");
  if (!std::isfinite(timeNs)) throw std::runtime_error("SnapshotWriter: a frame time is not finite");
  if (positionsNm.size() != 3 * species.size())
    throw std::runtime_error("SnapshotWriter: a frame needs 3 position values per molecule");
  for (const auto id : species)
    if (id >= fImpl->speciesCount) throw std::runtime_error("SnapshotWriter: species id outside the species table");
  try
  {
    auto frames = fImpl->file->openGroup("/frames");
    auto group = frames.createGroup(FrameName(fImpl->frames));
    group.createAttribute("time_ns", H5::PredType::NATIVE_DOUBLE, H5::DataSpace(H5S_SCALAR))
        .write(H5::PredType::NATIVE_DOUBLE, &timeNs);
    WriteDataset(group, "position_nm", H5::PredType::NATIVE_FLOAT, positionsNm.data(), species.size(), 3);
    WriteDataset(group, "species", H5::PredType::NATIVE_UINT8, species.data(), species.size(), 0);
    ++fImpl->frames;
  }
  catch (const H5::Exception& error)
  {
    throw std::runtime_error("SnapshotWriter: cannot write a frame to " + fImpl->path.string() + ": " +
                             error.getDetailMsg());
  }
}

std::size_t SnapshotWriter::FrameCount() const
{
  return fImpl ? fImpl->frames : 0;
}

void SnapshotWriter::Close()
{
  if (!fImpl || !fImpl->file) return;
  try
  {
    fImpl->file->close();
    fImpl->file.reset();
  }
  catch (const H5::Exception& error)
  {
    fImpl->file.reset();
    throw std::runtime_error("SnapshotWriter: cannot close " + fImpl->path.string() + ": " + error.getDetailMsg());
  }
}

SnapshotFile ReadSnapshotFile(const std::filesystem::path& path)
{
  if (!std::filesystem::exists(path)) throw std::runtime_error("SnapshotWriter: no such file " + path.string());
  H5::Exception::dontPrint();
  SnapshotFile result;
  try
  {
    H5::H5File file(path.string(), H5F_ACC_RDONLY);
    auto root = file.openGroup("/");
    root.openAttribute("formatVersion").read(H5::PredType::NATIVE_INT, &result.formatVersion);
    if (result.formatVersion != kSnapshotFormatVersion)
      throw std::runtime_error("SnapshotWriter: " + path.string() + " has format version " +
                               std::to_string(result.formatVersion) + ", expected " +
                               std::to_string(kSnapshotFormatVersion));

    auto speciesSet = root.openDataSet("species");
    hsize_t count = 0;
    speciesSet.getSpace().getSimpleExtentDims(&count);
    std::vector<char*> pointers(count, nullptr);
    const auto type = Utf8String();
    if (count > 0) speciesSet.read(pointers.data(), type);
    for (auto* pointer : pointers)
    {
      result.species.emplace_back(pointer != nullptr ? pointer : "");
      if (pointer != nullptr) H5free_memory(pointer);
    }

    auto frames = root.openGroup("frames");
    std::vector<std::string> names;
    for (hsize_t i = 0; i < frames.getNumObjs(); ++i) names.push_back(frames.getObjnameByIdx(i));
    std::sort(names.begin(), names.end());
    for (const auto& name : names)
    {
      auto group = frames.openGroup(name);
      SnapshotFrame frame;
      group.openAttribute("time_ns").read(H5::PredType::NATIVE_DOUBLE, &frame.timeNs);
      auto positions = group.openDataSet("position_nm");
      hsize_t dims[2] = {0, 0};
      positions.getSpace().getSimpleExtentDims(dims);
      frame.positionsNm.resize(static_cast<std::size_t>(dims[0]) * 3);
      frame.species.resize(static_cast<std::size_t>(dims[0]));
      if (dims[0] > 0)
      {
        positions.read(frame.positionsNm.data(), H5::PredType::NATIVE_FLOAT);
        group.openDataSet("species").read(frame.species.data(), H5::PredType::NATIVE_UINT8);
      }
      result.frames.push_back(std::move(frame));
    }
  }
  catch (const H5::Exception& error)
  {
    throw std::runtime_error("SnapshotWriter: cannot read " + path.string() + ": " + error.getDetailMsg());
  }
  return result;
}

}  // namespace drs
