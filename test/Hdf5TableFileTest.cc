/// \file Hdf5TableFileTest.cc
/// Dependencies: Standard library, project code and HDF5 C++; no Geant4.
/// \brief Plain-check unit tests for Hdf5TableFile: write tables, read every
/// dataset and attribute back.

#include "scoring/Hdf5TableFile.hh"

#include <H5Cpp.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
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

namespace
{
  std::string ReadStrAttr(H5::H5Object& obj, const char* name)
  {
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    H5::Attribute attr = obj.openAttribute(name);
    char* p = nullptr;
    attr.read(type, &p);
    std::string s = p ? p : "";
    if (p) H5free_memory(p);
    return s;
  }
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
#endif
  CHECK(Hdf5TableFile::Available());

  using CT = OutputTable::ColumnType;
  OutputTable full("Full", "full table");
  full.AddColumn("time_ns", "ns", "time", CT::Double);
  full.AddColumn("nEvents", "", "events", CT::Int64);
  full.AddColumn("species", "", "species name", CT::String);
  CHECK(full.AddRow({1.5, std::int64_t(3), std::string("OH")}));
  CHECK(full.AddRow({2.5, std::int64_t(-4), std::string("e_aq")}));

  OutputTable empty("Empty", "no rows");
  empty.AddColumn("x_nm", "nm", "position", CT::Double);
  empty.AddColumn("name", "", "label", CT::String);

  const auto dir = std::filesystem::temp_directory_path();
  const std::string path = (dir / "Hdf5TableFileTest.h5").string();
  std::string err;
  CHECK(Hdf5TableFile::Write(path, {full, empty}, err));

  {
    H5::H5File file(path, H5F_ACC_RDONLY);
    int version = 0;
    file.openAttribute("formatVersion").read(H5::PredType::NATIVE_INT, &version);
    CHECK(version == 1);

    H5::Group g = file.openGroup("/Full");
    CHECK(ReadStrAttr(g, "description") == "full table");

    H5::DataSet t = g.openDataSet("time_ns");
    CHECK(t.getDataType() == H5::PredType::NATIVE_DOUBLE);
    double tv[2] = {0, 0};
    t.read(tv, H5::PredType::NATIVE_DOUBLE);
    CHECK(tv[0] == 1.5 && tv[1] == 2.5);
    CHECK(ReadStrAttr(t, "unit") == "ns");
    CHECK(ReadStrAttr(t, "description") == "time");

    H5::DataSet n = g.openDataSet("nEvents");
    CHECK(n.getDataType() == H5::PredType::NATIVE_INT64);
    std::int64_t nv[2] = {0, 0};
    n.read(nv, H5::PredType::NATIVE_INT64);
    CHECK(nv[0] == 3 && nv[1] == -4);
    CHECK(ReadStrAttr(n, "unit").empty());
    CHECK(ReadStrAttr(n, "description") == "events");

    H5::DataSet s = g.openDataSet("species");
    H5::StrType st(H5::PredType::C_S1, H5T_VARIABLE);
    st.setCset(H5T_CSET_UTF8);
    CHECK(s.getDataType().getClass() == H5T_STRING);
    CHECK(s.getStrType().isVariableStr());
    CHECK(s.getStrType().getCset() == H5T_CSET_UTF8);
    char* sv[2] = {nullptr, nullptr};
    s.read(sv, st);
    CHECK(std::string(sv[0]) == "OH" && std::string(sv[1]) == "e_aq");
    H5free_memory(sv[0]);
    H5free_memory(sv[1]);
    CHECK(ReadStrAttr(s, "description") == "species name");

    H5::Group e = file.openGroup("/Empty");
    CHECK(ReadStrAttr(e, "description") == "no rows");
    for (const char* name : {"x_nm", "name"}) {
      H5::DataSet d = e.openDataSet(name);
      hsize_t dim = 99;
      d.getSpace().getSimpleExtentDims(&dim);
      CHECK(dim == 0);
    }
    H5::DataSet xd = e.openDataSet("x_nm");
    CHECK(ReadStrAttr(xd, "unit") == "nm");
  }

  CHECK(!Hdf5TableFile::Write((dir / "no_such_dir" / "x.h5").string(), {full}, err));
  CHECK(!err.empty());

  std::filesystem::remove(path);
  std::cout << "Hdf5TableFileTest passed\n";
  return 0;
}
