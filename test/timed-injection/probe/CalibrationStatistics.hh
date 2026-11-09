#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Calibration
{
inline constexpr std::size_t calibrationCount = 1024, holdoutCount = 256, pairCount = 128;
inline constexpr std::size_t bootstrapCount = 2000;
struct Interval { double lower = 0, upper = 0; };
struct EnergySummary
{
  std::size_t count = 0;
  double mean = 0, sampleSD = 0, standardError = 0, lower95 = 0, upper95 = 0;
  double relativeHalfWidth = 0, zeroFraction = 0;
  std::vector<double> quantiles; // min, 25%, median, 75%, max; linear interpolation
  bool precisionQualified = false;
};
EnergySummary SummarizeEnergy(const std::vector<double> &energyMeV);
double StudentT975(double degreesOfFreedom);
double PoissonWeight(unsigned count, double nominalMean, double variedMean);
double PoissonOverflow(double mean, unsigned maximum);
struct Support
{
  double effectiveSampleSize = 0, largestNormalizedWeight = 0;
  double overflow = 0, anyOverflow = 0, variedMean = 0;
  bool supported = false;
};
bool Supported(double ess, double largestWeight, double tail);
Support EvaluateSupport(const std::vector<unsigned> &, double nominalMean, double variedMean);
struct PairedOutcome
{
  std::uint64_t realizationID = 0;
  bool lowComplete = false, highComplete = false;
  unsigned count = 0;
  std::map<std::string, double> low, high;
};
struct MetricSummary
{
  double lowMean = 0, highMean = 0, difference = 0;
  Interval conditional, calibrationOnly, combined;
};
struct ComparisonSummary
{
  EnergySummary calibration;
  double nominalMean = 0;
  Interval calibrationMeanBootstrap;
  bool complete = false, propagationSupported = false;
  std::vector<std::string> refusalReasons;
  std::vector<Support> support; // nominal followed by every bootstrap replicate
  std::map<std::string, MetricSummary> metrics;
  std::uint64_t calibrationSeed = 0, pairSeed = 0;
};
ComparisonSummary AnalyzeComparison(const std::vector<double> &calibrationMeV,
                                    const std::vector<PairedOutcome> &, std::uint64_t bootstrapSeed);
struct HoldoutSummary
{
  EnergySummary holdout;
  double difference = 0, standardError = 0;
  Interval difference95; // independent Welch t interval
  bool complete = false;
};
HoldoutSummary CompareHoldout(const EnergySummary &, const std::vector<double> &);
} // namespace Calibration
