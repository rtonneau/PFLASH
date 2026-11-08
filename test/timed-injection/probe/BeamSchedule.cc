#include "BeamSchedule.hh"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace BeamSchedule
{
namespace
{
void Positive(double x)
{
  if (!std::isfinite(x) || x <= 0)
    throw std::runtime_error("invalid calibration");
}
} // namespace
double Rate(double doseRate, double massKg, double meanEnergyMeV)
{
  Positive(doseRate);
  Positive(massKg);
  Positive(meanEnergyMeV);
  double r = doseRate * massKg / (meanEnergyMeV * joulePerMeV);
  Positive(r);
  return r;
}
double Dose(double energyMeV, double massKg)
{
  Positive(massKg);
  if (!std::isfinite(energyMeV) || energyMeV < 0)
    throw std::runtime_error("invalid energy");
  return energyMeV * joulePerMeV / massKg;
}
std::vector<Arrival> Sample(double rate, double duration, std::size_t librarySize,
                            std::mt19937_64 &arrivals, std::mt19937_64 &selection)
{
  return SampleBounded(rate, duration, librarySize, arrivals, selection,
                       std::numeric_limits<std::size_t>::max());
}
std::vector<Arrival> SampleBounded(double rate, double duration, std::size_t librarySize,
                                   std::mt19937_64 &arrivals, std::mt19937_64 &selection,
                                   std::size_t maximum)
{
  Positive(rate);
  Positive(duration);
  if (!librarySize)
    throw std::runtime_error("empty library");
  std::exponential_distribution<double> wait(rate);
  std::uniform_int_distribution<std::size_t> choose(0, librarySize - 1);
  std::vector<Arrival> result;
  double t = 0;
  while (true)
  {
    double next = t + wait(arrivals);
    if (next >= duration)
      break;
    if (next <= t)
      throw std::runtime_error("unresolvable arrival time");
    t = next;
    if (result.size() == maximum)
      throw std::runtime_error("source cap exceeded; history censored");
    result.push_back({t, choose(selection)});
  }
  return result;
}
} // namespace BeamSchedule
