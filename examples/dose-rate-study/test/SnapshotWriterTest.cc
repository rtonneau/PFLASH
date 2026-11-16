#include "SnapshotWriter.hh"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "TestCheck.hh"

using namespace drs;

namespace
{

std::filesystem::path TestDirectory(const char* argument)
{
  const std::filesystem::path directory = argument;
  std::filesystem::create_directories(directory);
  return directory;
}

void TestRoundTrip(const std::filesystem::path& directory)
{
  const auto file = directory / "round-trip.h5";
  const std::vector<std::string> species{"H^0", "OH^0", "e_aq^-1"};
  {
    SnapshotWriter writer(file, species);
    writer.AddFrame(0.0025, {1.5F, -2.25F, 3.0F, 100.0F, 0.0F, -100.0F}, {0, 2});
    writer.AddFrame(0.005, {}, {});  // a frame with no molecule
    writer.AddFrame(1.0e6, {0.125F, 0.25F, 0.5F}, {1});
    CHECK(writer.FrameCount() == 3);
    writer.Close();
    writer.Close();  // closing twice is harmless
    CHECK_THROWS(writer.AddFrame(2.0e6, {}, {}), "closed");
  }

  const auto read = ReadSnapshotFile(file);
  CHECK(read.formatVersion == kSnapshotFormatVersion);
  CHECK(read.species == species);
  CHECK(read.frames.size() == 3);
  CHECK(read.frames[0].timeNs == 0.0025 && read.frames[1].timeNs == 0.005 && read.frames[2].timeNs == 1.0e6);
  CHECK((read.frames[0].positionsNm == std::vector<float>{1.5F, -2.25F, 3.0F, 100.0F, 0.0F, -100.0F}));
  CHECK((read.frames[0].species == std::vector<std::uint8_t>{0, 2}));
  CHECK(read.frames[1].positionsNm.empty() && read.frames[1].species.empty());
  CHECK((read.frames[2].positionsNm == std::vector<float>{0.125F, 0.25F, 0.5F}));
  CHECK((read.frames[2].species == std::vector<std::uint8_t>{1}));
}

void TestRejects(const std::filesystem::path& directory)
{
  SnapshotWriter writer(directory / "rejects.h5", {"A", "B"});
  CHECK_THROWS(writer.AddFrame(1.0, {1.0F, 2.0F}, {0}), "3 position values");
  CHECK_THROWS(writer.AddFrame(1.0, {1.0F, 2.0F, 3.0F}, {2}), "outside the species table");
  CHECK_THROWS(writer.AddFrame(std::nan(""), {}, {}), "not finite");
  CHECK(writer.FrameCount() == 0);  // a rejected frame leaves no group behind
  writer.AddFrame(1.0, {1.0F, 2.0F, 3.0F}, {1});
  CHECK(writer.FrameCount() == 1);

  CHECK_THROWS(SnapshotWriter(directory / "no-such-directory" / "x.h5", {"A"}), "cannot create");
  CHECK_THROWS(SnapshotWriter(directory / "many.h5", std::vector<std::string>(kMaxSnapshotSpecies + 1, "S")),
               "do not fit");
  CHECK_THROWS(ReadSnapshotFile(directory / "missing.h5"), "no such file");
  const auto notHdf5 = directory / "text.h5";
  std::ofstream(notHdf5) << "not an hdf5 file";
  CHECK_THROWS(ReadSnapshotFile(notHdf5), "cannot read");
}

// A repetitive frame must compress (the datasets are chunked, shuffled, deflated).
void TestCompression(const std::filesystem::path& directory)
{
  const auto file = directory / "compression.h5";
  const std::size_t molecules = 200000;
  {
    SnapshotWriter writer(file, {"A", "B"});
    writer.AddFrame(1.0, std::vector<float>(3 * molecules, 12.5F), std::vector<std::uint8_t>(molecules, 1));
  }
  const auto raw = molecules * (3 * sizeof(float) + 1);
  const auto size = std::filesystem::file_size(file);
  std::printf("compression: %zu raw bytes, %zu in the file\n", static_cast<std::size_t>(raw),
              static_cast<std::size_t>(size));
  // The gzip filter is optional in an HDF5 build (vcpkg: the hdf5[zlib] feature).
  if (SnapshotWriter::Compressed()) CHECK(size < raw / 10);
  const auto read = ReadSnapshotFile(file);
  CHECK(read.frames.size() == 1 && read.frames[0].species.size() == molecules);
  CHECK(read.frames[0].positionsNm.back() == 12.5F && read.frames[0].species.back() == 1);
}

}  // namespace

int main(int argc, char** argv)
{
  if (!SnapshotWriter::Available())
  {
    // The stub: every call is an error.
    CHECK_THROWS(SnapshotWriter("x.h5", {"A"}), "no HDF5");
    CHECK_THROWS(ReadSnapshotFile("x.h5"), "no HDF5");
    return 0;
  }
  const auto directory = TestDirectory(argc > 1 ? argv[1] : "snapshot-writer-test");
  TestRoundTrip(directory);
  TestRejects(directory);
  TestCompression(directory);
  return 0;
}
