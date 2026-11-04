/// \file EventStat.hh
/// \brief Running sum and sum of squares of one per-event quantity.
/// Standard library only.
///
/// Self-contained, project-agnostic unit: copy-paste portable to another
/// Geant4-DNA project (adjust the include path prefix on copy). Not thread
/// safe: callers merge per-thread instances.
///
/// Events that did not produce the quantity count as zero: the caller passes
/// the total event count n to Mean/StdErr, and only adds the non-zero values.

#ifndef EventStat_h
#define EventStat_h 1

#include <cstdint>

class EventStat
{
public:
  /// Fold one per-event value.
  void Add(double value);
  /// Fold another instance (equals feeding its values here one by one).
  void Merge(const EventStat &other);
  /// Reset to the empty state.
  void Clear();

  double Sum() const { return sum; }
  double SumSq() const { return sumSq; }

  /// Mean over n events (n = events folded in, zeros included); 0 when n < 1.
  double Mean(std::int64_t n) const { return Mean(sum, n); }
  /// Standard error of the mean over n events; 0 when n < 2.
  double StdErr(std::int64_t n) const { return StdErr(sum, sumSq, n); }

  /// Same formulas on plain sums, for callers that only hold the sums.
  static double Mean(double sum, std::int64_t n);
  static double StdErr(double sum, double sumSq, std::int64_t n);

private:
  double sum = 0.0;
  double sumSq = 0.0;
};

#endif
