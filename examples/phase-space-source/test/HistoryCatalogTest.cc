// Global history numbering over an ordered file list, range selection and
// its fatal cases, duplicate files, and concurrent reads.
//
//   HistoryCatalogTest <output dir>
#include "HistoryCatalog.hh"
#include "PhaseSpaceWriter.hh"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <thread>
#include <vector>

#include "TestCheck.hh"

using namespace phsp;
namespace fs = std::filesystem;

namespace
{

// `count` histories with event IDs 10, 20, ...; history h has h + 1 records whose
// track IDs encode (tag, h, r), so a read can be traced back to its source.
std::vector<RecordedHistory> MakeHistories(int tag, int count)
{
  std::vector<RecordedHistory> histories;
  for (int h = 0; h < count; ++h)
  {
    RecordedHistory history;
    history.stage1EventId = static_cast<std::uint64_t>(10 * (h + 1));
    history.inBoxDepositEv = 100.0 * tag + h;
    for (int r = 0; r <= h; ++r)
    {
      PhaseSpaceRecord record;
      record.pdg = 11;
      record.trackId = 10000 * tag + 100 * h + r + 1;
      record.kineticEnergyEv = 1000.0 + r;
      record.positionNm = {1.0 * h, 2.0 * r, -2000.0};
      record.direction = {0, 0, 1};
      record.timeNs = 0.5 * r;
      history.records.push_back(record);
    }
    histories.push_back(history);
  }
  return histories;
}

fs::path WriteFile(const fs::path& dir, const char* name, int tag, int count)
{
  FileDescription d;
  d.stage1Seed = static_cast<std::uint64_t>(tag);
  d.primariesFired = 100;
  d.boxSideNm = 4000.0;
  d.depthNm = 1.0e6;
  const fs::path path = dir / name;
  WritePhaseSpaceFile(path, d, MakeHistories(tag, count));
  return path;
}

}  // namespace

int main(int argc, char** argv)
{
  CHECK(argc == 2);
  const fs::path dir = argv[1];
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path a = WriteFile(dir, "a.phsp", 1, 2);
  const fs::path empty = WriteFile(dir, "empty.phsp", 2, 0);
  const fs::path c = WriteFile(dir, "c.phsp", 3, 3);
  fs::copy_file(a, dir / "a-copy.phsp");

  // Numbering: file order, then history index; an empty file takes no index.
  const HistoryCatalog catalog({a, empty, c});
  CHECK(catalog.FileCount() == 3);
  CHECK(catalog.HistoryCount() == 5);
  CHECK(catalog.FirstGlobalIndex(0) == 0 && catalog.FirstGlobalIndex(1) == 2 && catalog.FirstGlobalIndex(2) == 2);
  const std::uint64_t idA = catalog.File(0).Header().fileId;
  const std::uint64_t idC = catalog.File(2).Header().fileId;
  CHECK(idA != idC);
  CHECK(catalog.Locate(0) == (HistoryKey{0, idA, 0}));
  CHECK(catalog.Locate(1) == (HistoryKey{0, idA, 1}));
  CHECK(catalog.Locate(2) == (HistoryKey{2, idC, 0}));
  CHECK(catalog.Locate(4) == (HistoryKey{2, idC, 2}));

  const PhaseSpaceHistory h3 = catalog.ReadHistory(3);
  CHECK(h3.fileId == idC && h3.entry.historyIndex == 1 && h3.entry.stage1EventId == 20);
  CHECK(h3.records.size() == 2 && h3.records[1].trackId == 30102);

  // Another order is another numbering.
  const HistoryCatalog reversed({c, a});
  CHECK(reversed.Locate(0) == (HistoryKey{0, idC, 0}));
  CHECK(reversed.Locate(3) == (HistoryKey{1, idA, 0}));

  // Ranges.
  CHECK(catalog.Select(0, 5) == (HistoryRange{0, 5}));
  CHECK(catalog.Select(3, 2) == (HistoryRange{3, 2}));
  CHECK(catalog.Select(5, 0) == (HistoryRange{5, 0}));
  CHECK_THROWS(catalog.Select(4, 2), "goes past the end (5 histories in 3 files)");
  CHECK_THROWS(catalog.Select(6, 0), "goes past the end");
  CHECK_THROWS(catalog.Select(1, std::numeric_limits<std::uint64_t>::max()), "goes past the end");
  CHECK_THROWS(catalog.Locate(5), "global history 5 is past the end");
  CHECK_THROWS(catalog.ReadHistory(5), "past the end");
  CHECK_THROWS(catalog.File(3), "file position 3 does not exist");

  // Fatal lists.
  CHECK_THROWS(HistoryCatalog({}), "no phase-space file given");
  CHECK_THROWS(HistoryCatalog({a, c, a}), "given twice");
  CHECK_THROWS(HistoryCatalog({a, dir / "a-copy.phsp"}), "(position 0) and");
  CHECK_THROWS(HistoryCatalog({a, dir / "missing.phsp"}), "missing.phsp");

  // Concurrent reads through the shared readers give the serial result.
  std::vector<PhaseSpaceHistory> serial;
  for (std::uint64_t g = 0; g < catalog.HistoryCount(); ++g)
    serial.push_back(catalog.ReadHistory(g));
  std::vector<int> mismatches(4, 0);
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t)
    threads.emplace_back([&, t] {
      for (int pass = 0; pass < 200; ++pass)
        for (std::uint64_t g = 0; g < catalog.HistoryCount(); ++g)
        {
          const PhaseSpaceHistory h = catalog.ReadHistory((g + static_cast<std::uint64_t>(t)) % 5);
          const PhaseSpaceHistory& expected = serial[(g + static_cast<std::uint64_t>(t)) % 5];
          if (h.records != expected.records || !(h.entry == expected.entry) || h.fileId != expected.fileId)
            ++mismatches[static_cast<std::size_t>(t)];
        }
    });
  for (auto& thread : threads)
    thread.join();
  for (int m : mismatches)
    CHECK(m == 0);
  return 0;
}
