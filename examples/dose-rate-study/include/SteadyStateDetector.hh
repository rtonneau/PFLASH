// SteadyStateDetector: the Steady-state stop of a dose-cutoff history
// (Geant4-free, covered by test/SteadyStateDetectorTest.cc).
//
// The history gives one sample per arrival: the count of every species just
// before that arrival's source joins (ActivationObserver). Samples are grouped
// in consecutive, non-overlapping windows of `window` samples. When a window
// completes, its mean per species is compared with the previous window's: the
// steady state is reached when, for every species,
//     |mean_now - mean_before| < tolerance * mean_before,
// a species whose two means are both <= minMean being skipped (with the
// default minMean = 0, only a species absent from both windows). The earliest
// stop is therefore at the end of the second window. Once reached, the
// detector refuses further samples; SteadyMeans() (the mean of the last
// window) is the count N_ss used to extrapolate the history to beam-off.
#pragma once

#include <cstddef>
#include <vector>

namespace drs
{

struct SteadyStateSettings
{
  std::size_t window = 50;   // samples (arrivals) per window, >= 1
  double tolerance = 0.05;   // relative, > 0
  double minMean = 0;        // species with both window means <= this are skipped, >= 0
};

class SteadyStateDetector
{
 public:
  // speciesCount >= 1. Throws std::invalid_argument on invalid settings.
  SteadyStateDetector(const SteadyStateSettings& settings, std::size_t speciesCount);

  // One count per species, in a fixed order. Returns true when this sample
  // completes a window that matches the previous one (Reached() becomes true).
  // Throws std::invalid_argument on a wrong size or a negative or non-finite
  // count, std::logic_error once Reached().
  bool AddSample(const std::vector<double>& counts);

  bool Reached() const { return fReached; }
  std::size_t SampleCount() const { return fSamples; }
  // One entry per completed window: the largest relative change against the
  // previous window over the compared species (NaN for the first window, 0
  // when no species was compared).
  const std::vector<double>& WindowMaxRelativeChanges() const { return fWindowChanges; }
  // Mean of the last completed window (empty before the first one).
  const std::vector<double>& SteadyMeans() const { return fLastMean; }
  const SteadyStateSettings& Settings() const { return fSettings; }

  // The comparison rule above; also returns the largest relative change in
  // maxRelativeChange. Exposed for tests.
  static bool WindowsMatch(const std::vector<double>& before, const std::vector<double>& now,
                           const SteadyStateSettings& settings, double& maxRelativeChange);

 private:
  SteadyStateSettings fSettings;
  std::vector<double> fSum;       // current window
  std::size_t fInWindow = 0;
  std::size_t fSamples = 0;
  std::vector<double> fLastMean;  // last completed window
  std::vector<double> fWindowChanges;
  bool fReached = false;
};

}  // namespace drs
