/// \file MesoSpatialFileTest.cc
/// \brief Plain-assert unit tests for MesoSpatialFile (no test framework, no
/// Geant4 runtime -- HDF5 round trip, error paths and staging move).

#include "scoring/MesoSpatialFile.hh"

#include <H5Cpp.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;
using namespace MesoSpatialFile;

// assert() is compiled away under NDEBUG, so this test uses its own check.
#define CHECK(cond)                                                                     \
  do {                                                                                  \
    if (!(cond)) {                                                                      \
      std::cerr << "CHECK failed: " #cond " at line " << __LINE__ << std::endl;         \
      std::abort();                                                                     \
    }                                                                                   \
  } while (0)

namespace
{
  fs::path TestRoot()
  {
    return fs::temp_directory_path() / "MesoSpatialFileTest";
  }

  H5::StrType Utf8String()
  {
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    return type;
  }

  std::string ReadStringAttr(const H5::H5Object& obj, const char* name)
  {
    H5::Attribute attr = obj.openAttribute(name);
    H5::StrType type = Utf8String();
    char* ptr = nullptr;
    attr.read(type, &ptr);
    std::string value(ptr);
    H5free_memory(ptr);
    return value;
  }

  double ReadDoubleAttr(const H5::H5Object& obj, const char* name)
  {
    double v = 0.;
    obj.openAttribute(name).read(H5::PredType::NATIVE_DOUBLE, &v);
    return v;
  }
}

// Compares object identity with H5Oget_info (token on 1.12+, addr before).
static bool SameObject(const H5::H5File& file, const std::string& a, const std::string& b)
{
  H5O_info_t ia, ib;
#if H5_VERSION_GE(1, 12, 0)
  CHECK(H5Oget_info_by_name3(file.getId(), a.c_str(), &ia, H5O_INFO_BASIC, H5P_DEFAULT) >= 0);
  CHECK(H5Oget_info_by_name3(file.getId(), b.c_str(), &ib, H5O_INFO_BASIC, H5P_DEFAULT) >= 0);
  int cmp = 1;
  CHECK(H5Otoken_cmp(file.getId(), &ia.token, &ib.token, &cmp) >= 0);
  return cmp == 0;
#else
  CHECK(H5Oget_info_by_name(file.getId(), a.c_str(), &ia, H5P_DEFAULT) >= 0);
  CHECK(H5Oget_info_by_name(file.getId(), b.c_str(), &ib, H5P_DEFAULT) >= 0);
  return ia.addr == ib.addr;
#endif
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

  std::error_code ec;
  fs::remove_all(TestRoot(), ec);
  const std::string outDir = (TestRoot() / "out").string();

  CHECK(FileName() == "SpeciesMesoSpatial.h5");
  CHECK(StagingDir("out") == "out/.pending_meso_spatial");
  CHECK(StagingDir("") == ".pending_meso_spatial");
  CHECK(StagedPath("out") == "out/.pending_meso_spatial/SpeciesMesoSpatial.h5");
  CHECK(StagedPath("") == ".pending_meso_spatial/SpeciesMesoSpatial.h5");

  // "HO_2" + UTF-8 degree sign: Geant4 display names are not pure ASCII.
  const std::vector<std::string> species = {"H2O2", "e_aq", "HO_2\xC2\xB0"};
  const std::string path = StagedPath(outDir);
  std::string err;

  // Event 0: N = 0 cells, one record.
  EventData ev0;
  ev0.runId = 0;
  ev0.eventId = 0;
  ev0.snapshots.push_back(Snapshot{15.26, {}, {}});
  ev0.records.push_back({1.0, 0});
  CHECK(AppendEvent(path, species, ev0, err));
  CHECK(fs::is_regular_file(path));

  // Event 1: two records sharing snapshot 0, a third on snapshot 1.
  EventData ev1;
  ev1.runId = 0;
  ev1.eventId = 1;
  Snapshot a;
  a.cellSize_nm = 6.25;
  a.position_nm = {1, 2, 3, 4, 5, 6};
  a.counts = {1, 2, 3, 4, 5, 6};
  Snapshot b;
  b.cellSize_nm = 12.5;
  b.position_nm = {7, 8, 9};
  b.counts = {9, 8, 7};
  ev1.snapshots = {a, b};
  ev1.records = {{5.0, 0}, {10.0, 0}, {20.0, 1}};
  CHECK(AppendEvent(path, species, ev1, err));

  // Errors leave the file unchanged.
  const auto sizeBefore = fs::file_size(path);
  CHECK(!AppendEvent(path, species, ev1, err));
  CHECK(!err.empty());
  err.clear();
  CHECK(!AppendEvent(path, {"H2O2", "e_aq"}, ev0, err));
  CHECK(!err.empty());
  CHECK(fs::file_size(path) == sizeBefore);

  {
    H5::H5File f(path, H5F_ACC_RDONLY);
    H5::Group root = f.openGroup("/");
    CHECK(ReadStringAttr(root, "units") ==
          "position_nm: nm (cell centre, world frame); cellSize_nm: nm (cell side); "
          "time_ns: ns; counts: molecules per cell");
    int version = 0;
    root.openAttribute("formatVersion").read(H5::PredType::NATIVE_INT, &version);
    CHECK(version == 2);
    CHECK(version == kFormatVersion);
    CHECK(ReadStringAttr(root, "formatDoc") == kFormatDoc);

    H5::Attribute sp = root.openAttribute("species");
    hsize_t nsp = 0;
    sp.getSpace().getSimpleExtentDims(&nsp);
    CHECK(nsp == 3);
    std::vector<char*> ptrs(3, nullptr);
    sp.read(Utf8String(), ptrs.data());
    for (int i = 0; i < 3; ++i) {
      CHECK(species[i] == ptrs[i]);
      H5free_memory(ptrs[i]);
    }

    // Event 0: empty datasets.
    H5::Group g0 = f.openGroup("/run0/event0/snapshot0");
    CHECK(std::abs(ReadDoubleAttr(g0, "time_ns") - 1.0) < 1e-12);
    CHECK(std::abs(ReadDoubleAttr(g0, "cellSize_nm") - 15.26) < 1e-12);
    hsize_t dims[2] = {9, 9};
    f.openDataSet("/run0/event0/snapshot0/position_nm").getSpace().getSimpleExtentDims(dims);
    CHECK(dims[0] == 0 && dims[1] == 3);
    f.openDataSet("/run0/event0/snapshot0/counts").getSpace().getSimpleExtentDims(dims);
    CHECK(dims[0] == 0 && dims[1] == 3);

    // Event 1: values.
    std::vector<double> pos(6);
    H5::DataSet dpos = f.openDataSet("/run0/event1/snapshot0/position_nm");
    dpos.getSpace().getSimpleExtentDims(dims);
    CHECK(dims[0] == 2 && dims[1] == 3);
    // gzip only where the deflate filter exists; contiguous otherwise.
    CHECK(dpos.getCreatePlist().getNfilters() ==
          (H5Zfilter_avail(H5Z_FILTER_DEFLATE) > 0 ? 1 : 0));
    dpos.read(pos.data(), H5::PredType::NATIVE_DOUBLE);
    CHECK(pos == a.position_nm);
    std::vector<std::uint32_t> cnt(6);
    H5::DataSet dcnt = f.openDataSet("/run0/event1/snapshot0/counts");
    dcnt.getSpace().getSimpleExtentDims(dims);
    CHECK(dims[0] == 2 && dims[1] == 3);
    dcnt.read(cnt.data(), H5::PredType::NATIVE_UINT32);
    CHECK(cnt == a.counts);

    // Shared snapshot: distinct time_ns, hard-linked datasets.
    H5::Group g1 = f.openGroup("/run0/event1/snapshot1");
    CHECK(std::abs(ReadDoubleAttr(g1, "time_ns") - 10.0) < 1e-12);
    CHECK(std::abs(ReadDoubleAttr(g1, "cellSize_nm") - 6.25) < 1e-12);
    CHECK(SameObject(f, "/run0/event1/snapshot0/position_nm", "/run0/event1/snapshot1/position_nm"));
    CHECK(SameObject(f, "/run0/event1/snapshot0/counts", "/run0/event1/snapshot1/counts"));

    // Distinct snapshot is not linked.
    H5::Group g2 = f.openGroup("/run0/event1/snapshot2");
    CHECK(std::abs(ReadDoubleAttr(g2, "time_ns") - 20.0) < 1e-12);
    CHECK(!SameObject(f, "/run0/event1/snapshot0/counts", "/run0/event1/snapshot2/counts"));
    std::vector<double> pos2(3);
    f.openDataSet("/run0/event1/snapshot2/position_nm").read(pos2.data(), H5::PredType::NATIVE_DOUBLE);
    CHECK(pos2 == b.position_nm);
  }

  // MoveStaged: moves the file, then reports no file.
  const std::string target = (TestRoot() / "dump" / "p_SpeciesMesoSpatial.h5").string();
  bool moved = false;
  CHECK(MoveStaged(outDir, target, moved, err));
  CHECK(moved);
  CHECK(fs::is_regular_file(target));
  CHECK(!fs::exists(path));
  CHECK(MoveStaged(outDir, target, moved, err));
  CHECK(!moved);
  CHECK(fs::is_regular_file(target));

  fs::remove_all(TestRoot(), ec);
  std::cout << "All MesoSpatialFile tests passed." << std::endl;
  return 0;
}
