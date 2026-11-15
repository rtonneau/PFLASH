#include "SteadyStateDetector.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace drs
{

SteadyStateDetector::SteadyStateDetector(const SteadyStateSettings& settings, std::size_t speciesCount)
    : fSettings(settings), fSum(speciesCount, 0.0)
{
  if (settings.window < 1) throw std::invalid_argument("SteadyStateDetector: the window must hold at least 1 sample");
  if (!std::isfinite(settings.tolerance) || !(settings.tolerance > 0))
    throw std::invalid_argument("SteadyStateDetector: the tolerance must be positive and finite");
  if (!std::isfinite(settings.minMean) || !(settings.minMean >= 0))
    throw std::invalid_argument("SteadyStateDetector: minMean must be finite and not negative");
  if (speciesCount < 1) throw std::invalid_argument("SteadyStateDetector: no species to watch");
}

bool SteadyStateDetector::WindowsMatch(const std::vector<double>& before, const std::vector<double>& now,
                                       const SteadyStateSettings& settings, double& maxRelativeChange)
{
  if (before.size() != now.size()) throw std::invalid_argument("SteadyStateDetector: window sizes differ");
  bool match = true;
  maxRelativeChange = 0;
  for (std::size_t s = 0; s < now.size(); ++s)
  {
    if (std::max(before[s], now[s]) <= settings.minMean) continue;  // also skips 0 and 0
    const double change = std::abs(now[s] - before[s]);
    // before == 0 < now: an infinite relative change.
    const double relative = before[s] > 0 ? change / before[s] : std::numeric_limits<double>::infinity();
    maxRelativeChange = std::max(maxRelativeChange, relative);
    if (!(change < settings.tolerance * before[s])) match = false;
  }
  return match;
}

bool SteadyStateDetector::AddSample(const std::vector<double>& counts)
{
  if (fReached) throw std::logic_error("SteadyStateDetector: sample after the steady state was reached");
  if (counts.size() != fSum.size())
    throw std::invalid_argument("SteadyStateDetector: expected one count per species");
  for (const double c : counts)
    if (!std::isfinite(c) || c < 0) throw std::invalid_argument("SteadyStateDetector: a count is negative or not finite");
  for (std::size_t s = 0; s < counts.size(); ++s) fSum[s] += counts[s];
  ++fSamples;
  if (++fInWindow < fSettings.window) return false;

  std::vector<double> mean(fSum.size());
  for (std::size_t s = 0; s < fSum.size(); ++s) mean[s] = fSum[s] / static_cast<double>(fSettings.window);
  std::fill(fSum.begin(), fSum.end(), 0.0);
  fInWindow = 0;

  bool match = false;
  double change = std::numeric_limits<double>::quiet_NaN();
  if (!fLastMean.empty()) match = WindowsMatch(fLastMean, mean, fSettings, change);
  fWindowChanges.push_back(change);
  fLastMean = std::move(mean);
  fReached = match;
  return match;
}

}  // namespace drs
