/// \file EventStat.cc
/// \brief Implementation of EventStat (standard library only).
#include "scoring/EventStat.hh"

#include <cmath>

void EventStat::Add(double value)
{
  sum += value;
  sumSq += value * value;
}

void EventStat::Merge(const EventStat &other)
{
  sum += other.sum;
  sumSq += other.sumSq;
}

void EventStat::Clear()
{
  sum = 0.0;
  sumSq = 0.0;
}

double EventStat::Mean(double sum, std::int64_t n)
{
  return n < 1 ? 0.0 : sum / static_cast<double>(n);
}

double EventStat::StdErr(double sum, double sumSq, std::int64_t n)
{
  if (n < 2)
    return 0.0;
  const double dn = static_cast<double>(n);
  // Sample variance (n - 1) of the per-event values; rounding can push it
  // slightly below zero for identical values.
  double variance = (sumSq - sum * sum / dn) / (dn - 1.0);
  if (variance < 0.0)
    variance = 0.0;
  return std::sqrt(variance / dn);
}
