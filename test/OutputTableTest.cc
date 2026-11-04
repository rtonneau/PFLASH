/// \file OutputTableTest.cc
/// Dependencies: Standard library and portable project code; no Geant4 dependency. Optional MSVC
/// CRT diagnostics.
/// Thread safety: Sequential test process.
/// \brief Plain-assert unit tests for OutputTable, EventStat, SpeciesSchema and TableWriters.
#include "scoring/EventStat.hh"
#include "scoring/OutputTable.hh"
#include "scoring/TableWriters.hh"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace
{
  using T = OutputTable::ColumnType;

  bool Near(double a, double b) { return std::fabs(a - b) <= 1e-12 * (1.0 + std::fabs(b)); }

  OutputTable MakeSimple()
  {
    OutputTable t("Demo", "demo table");
    t.AddColumn("id", "", "identifier", T::Int64);
    t.AddColumn("energy_keV", "keV", "energy", T::Double);
    t.AddColumn("label", "", "label", T::String);
    return t;
  }

  void TestRowsAndTypes()
  {
    OutputTable t = MakeSimple();
    assert(t.Name() == "Demo" && t.Description() == "demo table");
    assert(t.Columns().size() == 3);
    assert(t.Columns()[1].unit == "keV" && t.Columns()[1].type == T::Double);
    assert(t.AddRow({std::int64_t{1}, 2.5, std::string("a")}));
    assert(t.Rows().size() == 1);

    // Rejected rows are checked through IsValidRow: AddRow asserts in Debug.
    assert(!t.IsValidRow({std::int64_t{1}, 2.5}));
    assert(!t.IsValidRow({std::int64_t{1}, 2.5, std::string("a"), 3.0}));
    assert(!t.IsValidRow({2.5, 2.5, std::string("a")}));
    assert(!t.IsValidRow({std::int64_t{1}, std::int64_t{2}, std::string("a")}));
    assert(!t.IsValidRow({std::int64_t{1}, 2.5, 3.0}));
    assert(t.IsValidRow({std::int64_t{7}, 0.0, std::string()}));
#ifdef NDEBUG
    assert(!t.AddRow({std::int64_t{1}}));
    assert(t.Rows().size() == 1);
#endif
  }

  void TestEventStat()
  {
    EventStat empty;
    assert(empty.Sum() == 0.0 && empty.SumSq() == 0.0);
    assert(empty.Mean(0) == 0.0 && empty.StdErr(0) == 0.0);
    assert(empty.StdErr(1) == 0.0);

    EventStat one;
    one.Add(3.0);
    assert(one.Sum() == 3.0 && one.SumSq() == 9.0);
    assert(Near(one.Mean(1), 3.0) && one.StdErr(1) == 0.0);

    // values 1, 2, 3, 4 (n = 4): mean 2.5, sample var 5/3, stderr sqrt(5/12)
    const double values[] = {1.0, 2.0, 3.0, 4.0};
    EventStat all, a, b;
    for (int i = 0; i < 4; ++i)
    {
      all.Add(values[i]);
      (i < 2 ? a : b).Add(values[i]);
    }
    a.Merge(b);
    assert(a.Sum() == all.Sum() && a.SumSq() == all.SumSq());
    assert(Near(all.Mean(4), 2.5));
    assert(Near(all.StdErr(4), std::sqrt(5.0 / 12.0)));
    assert(Near(a.StdErr(4), all.StdErr(4)));

    // zeros count: same sums over n = 8 events
    assert(Near(all.Mean(8), 10.0 / 8.0));

    EventStat same;
    same.Add(2.0);
    same.Add(2.0);
    same.Add(2.0);
    assert(same.StdErr(3) == 0.0);
    all.Clear();
    assert(all.Sum() == 0.0);
  }

  std::vector<SpeciesSchema::Row> SpeciesRows()
  {
    SpeciesSchema::Row r;
    r.time_ns = 1.5;
    r.species = "OH";
    r.nEvents = 4;
    r.countSum = 10.0;
    r.countSumSq = 30.0;
    r.gSum = 2.0;
    r.gSumSq = 1.5;
    SpeciesSchema::Row s = r;
    s.time_ns = 1e6;
    s.species = "H3Op";
    s.nEvents = 1;
    return {r, s};
  }

  void TestSpeciesSchema()
  {
    const OutputTable t = SpeciesSchema::Build("Species", "d", SpeciesRows());
    const char *names[] = {"time_ns", "species", "nEvents", "countSum", "countSumSq", "countMean",
                           "countStdErr", "gSum", "gSumSq", "gMean", "gStdErr"};
    assert(t.Columns().size() == 11);
    for (std::size_t i = 0; i < 11; ++i)
      assert(t.Columns()[i].name == names[i]);
    assert(t.Columns()[0].unit == "ns" && t.Columns()[7].unit == "molecules/100 eV");
    assert(t.Columns()[1].type == T::String && t.Columns()[2].type == T::Int64);
    assert(t.Rows().size() == 2);
    const auto &row = t.Rows()[0];
    assert(Near(std::get<double>(row[5]), 2.5));
    assert(Near(std::get<double>(row[6]), EventStat::StdErr(10.0, 30.0, 4)));
    assert(Near(std::get<double>(row[9]), 0.5));
    assert(std::get<double>(t.Rows()[1][6]) == 0.0);  // nEvents = 1
    assert(SpeciesSchema::MakeTable("x", "y").Columns().size() == 11);
  }

  std::vector<std::string> SplitCsv(const std::string &line)
  {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
      const char c = line[i];
      if (quoted)
      {
        if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
        {
          cur += '"';
          ++i;
        }
        else if (c == '"')
          quoted = false;
        else
          cur += c;
      }
      else if (c == '"')
        quoted = true;
      else if (c == ',')
      {
        out.push_back(cur);
        cur.clear();
      }
      else
        cur += c;
    }
    out.push_back(cur);
    return out;
  }

  void TestCsv()
  {
    OutputTable t = MakeSimple();
    t.AddRow({std::int64_t{1}, 0.1, std::string("plain")});
    t.AddRow({std::int64_t{-2}, 1.0 / 3.0, std::string("a,b \"q\"")});
    std::ostringstream os;
    TableWriters::WriteCsv(os, t);
    std::istringstream in(os.str());
    std::string line;
    std::getline(in, line);
    assert(line == "id,energy_keV,label");
    std::getline(in, line);
    assert(line == "1,0.1,plain");  // unpadded, unquoted
    std::getline(in, line);
    const auto f = SplitCsv(line);
    assert(f.size() == 3 && f[0] == "-2" && f[2] == "a,b \"q\"");
    assert(std::stod(f[1]) == 1.0 / 3.0);  // full precision round trip
    assert(line.find("\"a,b \"\"q\"\"\"") != std::string::npos);
  }

  void TestTxt()
  {
    OutputTable t = MakeSimple();
    t.AddRow({std::int64_t{1}, 1.0 / 3.0, std::string("a")});
    t.AddRow({std::int64_t{22}, 12345678.0, std::string("longer")});
    std::ostringstream os;
    TableWriters::WriteTxt(os, t, "Demo: a table");
    std::istringstream in(os.str());
    std::string line;
    std::getline(in, line);
    assert(line == "# Demo: a table");
    std::getline(in, line);
    assert(line == "# units: energy_keV=keV");

    // Parse back the way pandas does: sep=\s+, comment='#'.
    std::vector<std::vector<std::string>> fields;
    while (std::getline(in, line))
    {
      if (line.empty() || line[0] == '#')
        continue;
      std::istringstream ls(line);
      std::vector<std::string> f;
      std::string tok;
      while (ls >> tok)
        f.push_back(tok);
      fields.push_back(f);
    }
    assert(fields.size() == 3);
    assert(fields[0] == (std::vector<std::string>{"id", "energy_keV", "label"}));
    assert(fields[1].size() == 3 && fields[1][0] == "1" && fields[1][1] == "0.333333" &&
           fields[1][2] == "a");
    assert(fields[2][0] == "22" && fields[2][1] == "1.23457e+07" && fields[2][2] == "longer");

    // species table round trip: 11 whitespace-separated fields per row
    std::ostringstream so;
    TableWriters::WriteTxt(so, SpeciesSchema::Build("Species", "d", SpeciesRows()), "Species: x");
    std::istringstream sin(so.str());
    int dataRows = 0;
    while (std::getline(sin, line))
    {
      if (line[0] == '#')
        continue;
      std::istringstream ls(line);
      std::string tok;
      int n = 0;
      while (ls >> tok)
        ++n;
      assert(n == 11);
      ++dataRows;
    }
    assert(dataRows == 3);  // header + 2 rows

    // a string with spaces is quoted, so it stays one field
    OutputTable labels("reactionCatalog", "d");
    labels.AddColumn("reactionId", "", "id", OutputTable::ColumnType::Int64);
    labels.AddColumn("reaction", "", "label", OutputTable::ColumnType::String);
    labels.AddRow({std::int64_t{0}, std::string("e_aq + OH -> OHm")});
    labels.AddRow({std::int64_t{1}, std::string("say \"hi\"")});
    std::ostringstream lo;
    TableWriters::WriteTxt(lo, labels, "Reactions: x");
    const std::string ltxt = lo.str();
    assert(ltxt.find("\"e_aq + OH -> OHm\"") != std::string::npos);
    assert(ltxt.find("\"say \"\"hi\"\"\"") != std::string::npos);
  }
}

int main()
{
#if defined(_MSC_VER) && defined(_DEBUG)
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
  TestRowsAndTypes();
  TestEventStat();
  TestSpeciesSchema();
  TestCsv();
  TestTxt();
  std::cout << "OutputTableTest: all tests passed\n";
  return 0;
}
