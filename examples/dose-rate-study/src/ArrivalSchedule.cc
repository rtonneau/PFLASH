#include "ArrivalSchedule.hh"

#include <cmath>
#include <cstdlib>
#include <istream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace drs
{
namespace
{

[[noreturn]] void Fail(const std::string& message)
{
  throw std::runtime_error("ArrivalSchedule: " + message);
}

void Require(bool ok, const std::string& message)
{
  if (!ok) Fail(message);
}

bool ParseDouble(const std::string& token, double& value)
{
  char* end = nullptr;
  value = std::strtod(token.c_str(), &end);
  return end != token.c_str() && *end == '\0';
}

bool ParseIndex(const std::string& token, std::size_t& value)
{
  if (token.empty() || token[0] == '-' || token[0] == '+') return false;
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') return false;
  value = static_cast<std::size_t>(parsed);
  return true;
}

void CheckBounds(const ScheduleBounds& bounds)
{
  Require(bounds.libraryPacketCount > 0, "the packet library is empty");
  Require(std::isfinite(bounds.endpointNs) && bounds.endpointNs > 0, "endpoint must be positive and finite");
  Require(bounds.maxArrivals >= 1 && bounds.minArrivals <= bounds.maxArrivals,
          "inconsistent arrival count bounds");
}

// Uniform in [0, 1) from the top 53 bits of one raw 64-bit draw.
double Uniform01(std::mt19937_64& engine)
{
  return static_cast<double>(engine() >> 11) * (1.0 / 9007199254740992.0);
}

constexpr double kJoulePerEvLocal = 1.602176634e-19;  // same value as Ledger.hh kJoulePerEv

}  // namespace

std::vector<Arrival> ParseDeterministicSchedule(std::istream& in, const ScheduleBounds& bounds)
{
  CheckBounds(bounds);
  std::vector<Arrival> arrivals;
  std::set<std::size_t> usedPackets;
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(in, line))
  {
    ++lineNumber;
    const std::string where = "line " + std::to_string(lineNumber) + ": ";
    std::istringstream fields(line);
    std::string first;
    if (!(fields >> first) || first[0] == '#') continue;  // blank or comment

    std::vector<std::string> tokens{first};
    for (std::string t; fields >> t;) tokens.push_back(t);
    Require(tokens.size() == 5, where + "expected 5 fields (time_ns packet_index dx_um dy_um dz_um), found " +
                                    std::to_string(tokens.size()));
    Arrival a;
    a.sourceIndex = arrivals.size();
    Require(ParseDouble(tokens[0], a.timeNs) && std::isfinite(a.timeNs), where + "time is not a finite number");
    Require(ParseIndex(tokens[1], a.packetIndex), where + "packet index is not a non-negative integer");
    for (int axis = 0; axis < 3; ++axis)
    {
      double micrometres = 0;
      Require(ParseDouble(tokens[2 + axis], micrometres) && std::isfinite(micrometres),
              where + "offset is not a finite number");
      a.offsetNm[axis] = micrometres * 1000.0;
      Require(std::isfinite(a.offsetNm[axis]), where + "offset overflows");
    }
    Require(a.timeNs >= 0, where + "negative time");
    Require(a.timeNs < bounds.endpointNs, where + "time is not before the endpoint");
    Require(arrivals.empty() || a.timeNs >= arrivals.back().timeNs, where + "times are not nondecreasing");
    Require(a.packetIndex < bounds.libraryPacketCount, where + "packet index is outside the library");
    Require(usedPackets.insert(a.packetIndex).second, where + "packet index used twice (no reuse)");
    Require(arrivals.size() < bounds.maxArrivals, where + "more than the maximum number of arrivals");
    arrivals.push_back(a);
  }
  Require(arrivals.size() >= bounds.minArrivals,
          "schedule has " + std::to_string(arrivals.size()) + " arrival(s), fewer than the required minimum " +
              std::to_string(bounds.minArrivals));
  return arrivals;
}

std::vector<Arrival> SamplePoissonArrivals(const PoissonParameters& parameters, const ScheduleBounds& bounds)
{
  CheckBounds(bounds);
  Require(std::isfinite(parameters.rateHz) && parameters.rateHz > 0, "rate must be positive and finite");
  Require(std::isfinite(parameters.exposureS) && parameters.exposureS > 0,
          "exposure must be positive and finite");
  const double exposureNs = parameters.exposureS * 1e9;
  Require(exposureNs < bounds.endpointNs, "the endpoint must be later than the exposure window");
  for (int axis = 0; axis < 3; ++axis)
  {
    Require(std::isfinite(parameters.regionCenterNm[axis]), "non-finite region centre");
    Require(std::isfinite(parameters.regionHalfSizeNm[axis]) && parameters.regionHalfSizeNm[axis] >= 0,
            "region half size must be finite and non-negative");
  }

  std::mt19937_64 engine(parameters.sourceSeed);
  std::vector<Arrival> arrivals;
  double timeS = 0;
  for (;;)
  {
    timeS += -std::log1p(-Uniform01(engine)) / parameters.rateHz;
    if (!(timeS < parameters.exposureS)) break;  // half-open window [0, exposure)
    Require(arrivals.size() < bounds.maxArrivals,
            "censored: more than " + std::to_string(bounds.maxArrivals) +
                " arrivals in the exposure window; raise the cap or lower rate*exposure");
    Require(arrivals.size() < bounds.libraryPacketCount,
            "library exhausted: arrival " + std::to_string(arrivals.size() + 1) + " needs a packet but only " +
                std::to_string(bounds.libraryPacketCount) + " exist (packets are never reused)");
    Arrival a;
    a.sourceIndex = arrivals.size();
    a.packetIndex = arrivals.size();
    a.timeNs = timeS * 1e9;
    for (int axis = 0; axis < 3; ++axis)
      a.offsetNm[axis] = parameters.regionCenterNm[axis] +
                         parameters.regionHalfSizeNm[axis] * (2.0 * Uniform01(engine) - 1.0);
    arrivals.push_back(a);
  }
  return arrivals;
}

ArrivalRates ComputeArrivalRates(double doseRateGyPerS, double doseGy, double boxHalfSideNm,
                                 const std::vector<double>& packetDepositsEv, std::size_t emptyCount,
                                 double emptyDepositSumEv)
{
  Require(std::isfinite(doseRateGyPerS) && doseRateGyPerS > 0, "the dose rate must be positive and finite");
  Require(std::isfinite(doseGy) && doseGy > 0, "the dose must be positive and finite");
  Require(std::isfinite(boxHalfSideNm) && boxHalfSideNm > 0, "the box half side must be positive and finite");
  Require(!packetDepositsEv.empty(), "the packet library is empty");
  Require(std::isfinite(emptyDepositSumEv) && emptyDepositSumEv >= 0,
          "the deposit of the empty crossings must be finite and not negative");

  ArrivalRates r;
  r.doseRateGyPerS = doseRateGyPerS;
  r.targetDoseGy = doseGy;
  r.boxHalfSideNm = boxHalfSideNm;
  const double sideM = 2.0 * boxHalfSideNm * 1e-9;
  r.boxMassKg = sideM * sideM * sideM * kWaterDensityKgPerM3;
  r.targetEnergyEv = doseGy * r.boxMassKg / kJoulePerEvLocal;
  r.packetCount = packetDepositsEv.size();
  r.emptyCount = emptyCount;
  r.launchedCount = r.packetCount + emptyCount;
  r.hitFraction = static_cast<double>(r.packetCount) / static_cast<double>(r.launchedCount);
  for (const double deposit : packetDepositsEv)
  {
    Require(std::isfinite(deposit) && deposit >= 0, "a packet deposit is negative or not finite");
    r.packetDepositSumEv += deposit;
  }
  Require(r.packetDepositSumEv > 0, "the packets deposit no energy");
  r.emptyDepositSumEv = emptyDepositSumEv;
  r.meanDepositPerLaunchedEv = r.packetDepositSumEv / static_cast<double>(r.launchedCount);
  r.meanDepositPerPacketEv = r.packetDepositSumEv / static_cast<double>(r.packetCount);
  // Ddot [J/(kg s)] * m [kg] / (ebar [eV] * e [J/eV]).
  r.launchedRateHz = doseRateGyPerS * r.boxMassKg / (r.meanDepositPerLaunchedEv * kJoulePerEvLocal);
  r.packetRateHz = r.hitFraction * r.launchedRateHz;
  return r;
}

DoseCutoffSchedule SampleDoseCutoffArrivals(const DoseCutoffParameters& parameters,
                                            const std::vector<double>& packetDepositsEv, std::size_t maxArrivals)
{
  Require(std::isfinite(parameters.rateHz) && parameters.rateHz > 0, "rate must be positive and finite");
  Require(std::isfinite(parameters.targetEnergyEv) && parameters.targetEnergyEv > 0,
          "the target energy must be positive and finite");
  Require(maxArrivals >= 1, "the maximum number of arrivals must be at least 1");
  for (const double deposit : packetDepositsEv)
    Require(std::isfinite(deposit) && deposit >= 0, "a packet deposit is negative or not finite");

  std::mt19937_64 engine(parameters.sourceSeed);
  DoseCutoffSchedule schedule;
  double timeS = 0;
  double energy = 0;
  while (!(energy >= parameters.targetEnergyEv))
  {
    const std::size_t k = schedule.arrivals.size();
    if (k > 0) timeS += -std::log1p(-Uniform01(engine)) / parameters.rateHz;  // the first arrival is at 0
    Require(k < maxArrivals, "censored: more than " + std::to_string(maxArrivals) +
                                 " arrivals before the target dose; raise maxArrivals or lower the dose");
    const std::size_t packet = parameters.firstPacket + k;
    Require(packet < packetDepositsEv.size(),
            "library exhausted: arrival " + std::to_string(k + 1) + " needs packet " + std::to_string(packet) +
                " but only " + std::to_string(packetDepositsEv.size()) +
                " exist (packets are never reused); the target dose was not reached");
    Arrival a;
    a.sourceIndex = k;
    a.packetIndex = packet;
    a.timeNs = timeS * 1e9;
    schedule.arrivals.push_back(a);
    energy += packetDepositsEv[packet];
    schedule.cumulativeEnergyEv.push_back(energy);
  }
  return schedule;
}

DoseEnergyChecks CheckDoseEnergy(const DoseCutoffSchedule& schedule, double targetEnergyEv,
                                 double energyAtBeamOffEv, double injectedDepositEv)
{
  DoseEnergyChecks checks;
  if (schedule.cumulativeEnergyEv.empty()) return checks;
  const std::vector<double>& energies = schedule.cumulativeEnergyEv;
  const double finalEnergy = energies.back();
  const double previous = energies.size() > 1 ? energies[energies.size() - 2] : 0;
  const auto same = [finalEnergy](double x) { return std::abs(x - finalEnergy) <= 1e-12 * std::abs(finalEnergy); };
  checks.targetReached = finalEnergy >= targetEnergyEv;
  checks.overshootBelowOneTrack = previous < targetEnergyEv;
  checks.beamOffHoldsEverySource = same(energyAtBeamOffEv);
  checks.injectedMatchesSchedule = same(injectedDepositEv);
  return checks;
}

}  // namespace drs
