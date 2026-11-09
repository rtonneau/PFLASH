#pragma once
#include <random>
#include <vector>
namespace BeamSchedule
{
inline constexpr double joulePerMeV = 1.602176634e-13;
double Rate(double doseRate, double massKg, double meanEnergyMeV);
double Dose(double energyMeV, double massKg);
struct Arrival
{
  double time;
  std::size_t packet;
};
std::vector<Arrival> Sample(double rate, double duration, std::size_t librarySize,
                            std::mt19937_64 &arrivals, std::mt19937_64 &selection);
std::vector<Arrival> SampleBounded(double rate, double duration, std::size_t librarySize,
                                   std::mt19937_64 &arrivals, std::mt19937_64 &selection,
                                   std::size_t maximum);
} // namespace BeamSchedule
