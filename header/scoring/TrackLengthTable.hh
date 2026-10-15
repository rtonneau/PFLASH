/// \file TrackLengthTable.hh
/// \brief Portable per-event table of primary and secondary track lengths
///
/// Self-contained, project-agnostic unit: standard library only, no Geant4
/// kernel and no other dnachem-min class. Copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy).
///
/// One Row per event. Lengths in nm, energies in keV. Callers fill the rows;
/// the table only stores, merges, sorts and writes them.

#ifndef TrackLengthTable_h
#define TrackLengthTable_h 1

#include <cstddef>
#include <iosfwd>
#include <vector>

class TrackLengthTable
{
 public:
  /// Why the primary track ended.
  enum class End
  {
    Stopped,
    Escaped,
    Killed
  };

  struct Row
  {
    int run = 0;
    int event = 0;
    double primaryLength_nm = 0.;
    double primaryEkin0_keV = 0.;
    double primaryEkinEnd_keV = 0.;
    End primaryEnd = End::Stopped;
    double secondaryFirstGen_nm = 0.;
    long nSecondaryFirstGen = 0;
    double secondaryAll_nm = 0.;
    long nSecondaryAll = 0;
  };

  /// Mean and standard error of the mean of one numeric column.
  struct Stat
  {
    double mean = 0.;
    double sem = 0.;
  };

  struct Summary
  {
    std::size_t nEvents = 0;
    Stat primaryLength_nm;
    Stat primaryEkin0_keV;
    Stat primaryEkinEnd_keV;
    Stat secondaryFirstGen_nm;
    Stat nSecondaryFirstGen;
    Stat secondaryAll_nm;
    Stat nSecondaryAll;
    std::size_t nStopped = 0;
    std::size_t nEscaped = 0;
    std::size_t nKilled = 0;
  };

  /// "stopped", "escaped" or "killed".
  static const char* EndToString(End end);

  void Add(const Row& row);

  /// Appends the rows of `other`; `other` is left unchanged.
  void Merge(const TrackLengthTable& other);

  void Clear();
  std::size_t Size() const { return fRows.size(); }
  bool Empty() const { return fRows.empty(); }

  /// Rows in insertion order.
  const std::vector<Row>& GetRows() const { return fRows; }

  /// Mean and SEM (sample stddev / sqrt(n); 0 for n < 2) of every column.
  Summary Summarize() const;

  /// Header row plus one row per event, sorted by (run, event).
  void WriteCsv(std::ostream& out) const;

  /// Same columns, whitespace-separated, `#`-commented header.
  void WriteAscii(std::ostream& out) const;

 private:
  std::vector<Row> Sorted() const;

  std::vector<Row> fRows;
};

#endif  // TrackLengthTable_h
