/// \file TrackLengthTable.hh
/// \brief Store and summarize per-event primary and secondary track lengths.
/// Standard library only.
/// Callers coordinate access to shared mutable tables and streams.
///
/// Self-contained, project-agnostic unit: standard library only, no Geant4
/// kernel; its only project dependency is OutputTable.hh. Copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy).
///
/// One Row per event. Lengths in nm, energies in keV. Callers fill the rows;
/// the table only stores, merges, sorts and writes them.

#ifndef TrackLengthTable_h
#define TrackLengthTable_h 1

#include "scoring/OutputTable.hh"

#include <cstddef>
#include <vector>

/// Caller-populated rows; no physical-value validation. Allocations may throw.
/// Writers report failure through stream flags or enabled stream exceptions.
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

  /// One event: lengths in nm, energies in keV, secondary counts dimensionless.
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

  /// Column statistics in the corresponding Row units, plus event/end counts.
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

  /// Borrow "stopped", "escaped" or "killed"; unknown enum values give "stopped".
  static const char* EndToString(End end);

  /// Append row without validating its values or enforcing unique event IDs.
  void Add(const Row& row);

  /// Append a distinct table's rows; other is left unchanged. Self-merge is unsupported.
  void Merge(const TrackLengthTable& other);

  /// Remove all rows.
  void Clear();
  /// Return the number of stored rows.
  std::size_t Size() const { return fRows.size(); }
  /// True when there are no rows.
  bool Empty() const { return fRows.empty(); }

  /// Rows in insertion order.
  const std::vector<Row>& GetRows() const { return fRows; }

  /// Mean and SEM (sample stddev / sqrt(n)) of every numeric column.
  /// Empty-table means are zero; one row retains its value. SEM is zero for n < 2.
  Summary Summarize() const;

  /// The `trackLengths` table, one row per event sorted by (run, event).
  /// Columns: run, event, primaryLength_nm, primaryEkinStart_keV,
  /// primaryEkinEnd_keV, primaryEnd (string), secondaryFirstGenLength_nm,
  /// nSecondaryFirstGen, secondaryAllLength_nm, nSecondaryAll.
  OutputTable ToTable() const;

 private:
  std::vector<Row> Sorted() const;

  std::vector<Row> fRows;
};

#endif  // TrackLengthTable_h
