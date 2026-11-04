/// \file RootTableFileTest.cc
/// Dependencies: Standard library, project code and g4tools; no Geant4 kernel.
/// \brief Plain-check unit tests for RootTableFile: write two tables, read
/// them back with tools::rroot and compare row counts, values, titles.

#include "scoring/RootTableFile.hh"

#include "tools/rroot/file"
#include "tools/rroot/fac"
#include "tools/rroot/tree"
#include "tools/rroot/streamers"
#include "tools/rroot/rall"
#include "tools/rroot/ntuple"
#include "tools/ntuple_binding"
#include "toolx/zlib"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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
  struct TreeHolder
  {
    std::unique_ptr<tools::rroot::fac> fac;
    std::unique_ptr<tools::rroot::tree> tree;
    std::unique_ptr<tools::rroot::ntuple> ntuple;
  };

  TreeHolder OpenTree(tools::rroot::file& file, const std::string& name,
                      const tools::ntuple_binding& binding = tools::ntuple_binding())
  {
    TreeHolder h;
    tools::rroot::key* key = file.dir().find_key(name);
    CHECK(key != nullptr);
    unsigned int size = 0;
    char* buf = key->get_object_buffer(file, size);
    CHECK(buf != nullptr);
    auto* buffer = new tools::rroot::buffer(std::cerr, file.byte_swap(), size, buf,
                                            key->key_length(), false);
    buffer->set_map_objs(true);
    h.fac.reset(new tools::rroot::fac(std::cerr));
    h.tree.reset(new tools::rroot::tree(file, *h.fac));
    CHECK(h.tree->stream(*buffer));
    delete buffer;
    h.ntuple.reset(new tools::rroot::ntuple(*h.tree));
    CHECK(h.ntuple->initialize(std::cerr, binding));
    return h;
  }
}

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
#endif
  using CT = OutputTable::ColumnType;
  OutputTable full("Full", "full table");
  full.AddColumn("time_ns", "ns", "time", CT::Double);
  full.AddColumn("nEvents", "", "events", CT::Int64);
  full.AddColumn("species", "", "species name", CT::String);
  CHECK(full.AddRow({1.5, std::int64_t(3), std::string("OH")}));
  CHECK(full.AddRow({2.5, std::int64_t(-4000000000LL), std::string("e_aq")}));
  CHECK(full.AddRow({3.5, std::int64_t(7), std::string("")}));

  OutputTable empty("Empty", "no rows");
  empty.AddColumn("x_nm", "nm", "position", CT::Double);

  const auto dir = std::filesystem::temp_directory_path();
  const std::string path = (dir / "RootTableFileTest.root").string();
  std::string err;
  CHECK(RootTableFile::Write(path, {full, empty}, err));

  {
    tools::rroot::file file(std::cerr, path);
    file.add_unziper('Z', toolx::decompress_buffer);
    CHECK(file.is_open());

    double tv = 0;
    double nv = 0;  // int64 is stored as a double
    std::string sv;
    tools::ntuple_binding binding;
    binding.add_column("time_ns", tv);
    binding.add_column("nEvents", nv);
    binding.add_column("species", sv);
    TreeHolder f = OpenTree(file, "Full", binding);
    CHECK(f.tree->title() == "full table");
    uint64_t entries = 0;
    CHECK(f.ntuple->number_of_entries(entries) && entries == 3);
    CHECK(f.tree->find_leaf("time_ns")->title() == "time_ns [ns]");
    CHECK(f.tree->find_leaf("nEvents")->title() == "nEvents");

    const double expectT[3] = {1.5, 2.5, 3.5};
    const double expectN[3] = {3, -4000000000.0, 7};
    const char* expectS[3] = {"OH", "e_aq", ""};
    f.ntuple->start();
    for (int i = 0; i < 3; ++i) {
      CHECK(f.ntuple->next());
      CHECK(f.ntuple->get_row());
      CHECK(tv == expectT[i]);
      CHECK(nv == expectN[i]);
      CHECK(sv == expectS[i]);
    }
    CHECK(!f.ntuple->next());

    TreeHolder e = OpenTree(file, "Empty");
    CHECK(e.tree->title() == "no rows");
    CHECK(e.tree->entries() == 0);
    CHECK(e.tree->find_leaf("x_nm")->title() == "x_nm [nm]");
  }

  OutputTable big("Big", "too big");
  big.AddColumn("n", "", "count", CT::Int64);
  CHECK(big.AddRow({std::int64_t(1) << 60}));
  CHECK(!RootTableFile::Write(path, {big}, err));
  CHECK(!err.empty());

  CHECK(!RootTableFile::Write((dir / "no_such_dir" / "x.root").string(), {full}, err));
  CHECK(!err.empty());

  std::filesystem::remove(path);
  std::cout << "RootTableFileTest passed\n";
  return 0;
}
