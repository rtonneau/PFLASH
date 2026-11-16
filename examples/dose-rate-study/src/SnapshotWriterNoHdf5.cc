// The SnapshotWriter interface for a build without HDF5: no snapshot file can
// be written or read, and `history` rejects `snapshots = true`.
#include "SnapshotWriter.hh"

#include <stdexcept>

namespace drs
{

namespace
{
[[noreturn]] void NoHdf5()
{
  throw std::runtime_error("SnapshotWriter: this build has no HDF5 (configure with -DDRS_WITH_HDF5=AUTO or ON)");
}
}  // namespace

struct SnapshotWriter::Impl
{
};

bool SnapshotWriter::Compressed()
{
  return false;
}

bool SnapshotWriter::Available()
{
  return false;
}

SnapshotWriter::SnapshotWriter(const std::filesystem::path&, const std::vector<std::string>&)
{
  NoHdf5();
}

SnapshotWriter::~SnapshotWriter() = default;

void SnapshotWriter::AddFrame(double, const std::vector<float>&, const std::vector<std::uint8_t>&)
{
  NoHdf5();
}

std::size_t SnapshotWriter::FrameCount() const
{
  return 0;
}

void SnapshotWriter::Close() {}

SnapshotFile ReadSnapshotFile(const std::filesystem::path&)
{
  NoHdf5();
}

}  // namespace drs
