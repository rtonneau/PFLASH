#include "ArrivalSchedule.hh"

#include <cmath>
#include <sstream>

#include "TestCheck.hh"

using namespace drs;

namespace
{

ScheduleBounds Bounds(std::size_t library = 10)
{
  ScheduleBounds b;
  b.libraryPacketCount = library;
  b.endpointNs = 3.0e6;  // 3 ms
  return b;
}

std::vector<Arrival> Parse(const std::string& text, const ScheduleBounds& b = Bounds())
{
  std::istringstream in(text);
  return ParseDeterministicSchedule(in, b);
}

void TestDeterministicParsing()
{
  const auto arrivals = Parse(
      "# time_ns packet_index dx_um dy_um dz_um\n"
      "\n"
      "0 0 0 0 0\n"
      "   # indented comment\n"
      "5e5 1 100 -50.5 0\n"
      "5e5 2 0 0 0\n"  // equal times are allowed (nondecreasing)
      "1000000 7 1e2 0 25\n");
  CHECK(arrivals.size() == 4);
  CHECK(arrivals[1].sourceIndex == 1);
  CHECK(arrivals[1].timeNs == 5e5);
  CHECK(arrivals[1].packetIndex == 1);
  CHECK(arrivals[1].offsetNm[0] == 100000.0);
  CHECK(arrivals[1].offsetNm[1] == -50500.0);
  CHECK(arrivals[3].packetIndex == 7);
  CHECK(arrivals[3].offsetNm[2] == 25000.0);
}

void TestDeterministicRejections()
{
  CHECK_THROWS(Parse(""), "fewer than the required minimum");
  CHECK_THROWS(Parse("# only comments\n\n"), "fewer than the required minimum");
  CHECK_THROWS(Parse("0 0 0 0\n"), "expected 5 fields");
  CHECK_THROWS(Parse("0 0 0 0 0 0\n"), "expected 5 fields");
  CHECK_THROWS(Parse("abc 0 0 0 0\n"), "time is not a finite");
  CHECK_THROWS(Parse("nan 0 0 0 0\n"), "time is not a finite");
  CHECK_THROWS(Parse("inf 0 0 0 0\n"), "time is not a finite");
  CHECK_THROWS(Parse("0 0 nan 0 0\n"), "offset is not a finite");
  CHECK_THROWS(Parse("0 0 0 0 1e400\n"), "offset is not a finite");
  CHECK_THROWS(Parse("0 1.5 0 0 0\n"), "packet index");
  CHECK_THROWS(Parse("0 -1 0 0 0\n"), "packet index");
  CHECK_THROWS(Parse("-1 0 0 0 0\n"), "negative time");
  CHECK_THROWS(Parse("3000000 0 0 0 0\n"), "before the endpoint");  // endpoint itself is excluded
  CHECK_THROWS(Parse("4000000 0 0 0 0\n"), "before the endpoint");
  CHECK_THROWS(Parse("100 0 0 0 0\n50 1 0 0 0\n"), "nondecreasing");
  CHECK_THROWS(Parse("0 10 0 0 0\n"), "outside the library");
  CHECK_THROWS(Parse("0 3 0 0 0\n1 3 0 0 0\n"), "used twice");
  CHECK_THROWS(Parse("bad line here and\n"), "expected 5 fields");

  // The error names the offending line.
  CHECK_THROWS(Parse("0 0 0 0 0\n1 1 0 0 0\nx 2 0 0 0\n"), "line 3");

  ScheduleBounds capped = Bounds();
  capped.maxArrivals = 2;
  CHECK_THROWS(Parse("0 0 0 0 0\n1 1 0 0 0\n2 2 0 0 0\n", capped), "maximum number");
  CHECK(Parse("0 0 0 0 0\n1 1 0 0 0\n", capped).size() == 2);

  ScheduleBounds needTwo = Bounds();
  needTwo.minArrivals = 2;
  CHECK_THROWS(Parse("0 0 0 0 0\n", needTwo), "fewer than the required minimum");

  ScheduleBounds noLibrary = Bounds(0);
  CHECK_THROWS(Parse("0 0 0 0 0\n", noLibrary), "library is empty");
  ScheduleBounds badEndpoint = Bounds();
  badEndpoint.endpointNs = 0;
  CHECK_THROWS(Parse("0 0 0 0 0\n", badEndpoint), "endpoint");
}

PoissonParameters Params(double rateHz, double exposureS, std::uint64_t seed)
{
  PoissonParameters p;
  p.rateHz = rateHz;
  p.exposureS = exposureS;
  p.sourceSeed = seed;
  return p;
}

void TestPoissonBasics()
{
  const PoissonParameters p = Params(2000.0, 2.0e-3, 12345);
  const auto a = SamplePoissonArrivals(p, Bounds(1000));
  const auto b = SamplePoissonArrivals(p, Bounds(1000));
  CHECK(a == b);  // reproducible by seed

  bool differs = false;
  for (std::uint64_t seed = 1; seed <= 10 && !differs; ++seed)
  {
    PoissonParameters q = p;
    q.sourceSeed = 12345 + seed;
    differs = !(SamplePoissonArrivals(q, Bounds(1000)) == a);
  }
  CHECK(differs);

  // Structure: sorted, in [0, exposure), packets 0,1,2..., offsets inside the region.
  PoissonParameters wide = Params(1.0e5, 2.0e-3, 7);
  wide.regionCenterNm = {1000, -2000, 0};
  wide.regionHalfSizeNm = {500, 0, 50000};
  const auto many = SamplePoissonArrivals(wide, Bounds(5000));
  CHECK(many.size() > 50);
  double previous = 0;
  for (std::size_t i = 0; i < many.size(); ++i)
  {
    const Arrival& x = many[i];
    CHECK(x.sourceIndex == i && x.packetIndex == i);
    CHECK(x.timeNs >= previous);
    CHECK(x.timeNs >= 0 && x.timeNs < 2.0e-3 * 1e9);
    previous = x.timeNs;
    CHECK(std::abs(x.offsetNm[0] - 1000) <= 500);
    CHECK(x.offsetNm[1] == -2000);
    CHECK(std::abs(x.offsetNm[2]) <= 50000);
  }
}

void TestPoissonStatistics()
{
  // rate*exposure = 4: counts ~ Poisson(4), many samples, loose 5-sigma bounds.
  const double lambda = 4.0;
  const int samples = 4000;
  double sum = 0, sumSquares = 0;
  for (int s = 0; s < samples; ++s)
  {
    const auto arrivals = SamplePoissonArrivals(Params(2000.0, lambda / 2000.0, 1000 + s), Bounds(1000));
    const double n = static_cast<double>(arrivals.size());
    sum += n;
    sumSquares += n * n;
  }
  const double mean = sum / samples;
  const double variance = (sumSquares - samples * mean * mean) / (samples - 1);
  const double meanError = std::sqrt(lambda / samples);
  CHECK(std::abs(mean - lambda) < 5 * meanError);
  // Variance of the sample variance of a Poisson(4): (mu4 - sigma^4)/n with mu4 = lambda + 3 lambda^2.
  const double varianceError = std::sqrt((lambda + 3 * lambda * lambda - lambda * lambda) / samples);
  CHECK(std::abs(variance - lambda) < 5 * varianceError);
}

void TestPoissonEdgeCases()
{
  // Zero arrivals is valid: tiny rate*exposure, scan seeds until one is empty.
  bool sawEmpty = false;
  for (std::uint64_t seed = 1; seed <= 20; ++seed)
    if (SamplePoissonArrivals(Params(1.0, 1e-3, seed), Bounds(1)).empty()) sawEmpty = true;
  CHECK(sawEmpty);

  // Exhausted library: hard error even though it could be satisfied by reuse.
  CHECK_THROWS(SamplePoissonArrivals(Params(1.0e6, 2.0e-3, 3), Bounds(5)), "library exhausted");
  // Exactly enough packets is fine.
  const auto n = SamplePoissonArrivals(Params(2000.0, 2.0e-3, 99), Bounds(1000)).size();
  CHECK(SamplePoissonArrivals(Params(2000.0, 2.0e-3, 99), Bounds(n)).size() == n);
  if (n > 0) CHECK_THROWS(SamplePoissonArrivals(Params(2000.0, 2.0e-3, 99), Bounds(n - 1)), "library exhausted");

  // Safety cap throws instead of truncating.
  ScheduleBounds capped = Bounds(100000);
  capped.maxArrivals = 3;
  CHECK_THROWS(SamplePoissonArrivals(Params(1.0e6, 2.0e-3, 3), capped), "censored");

  // Parameter validation.
  CHECK_THROWS(SamplePoissonArrivals(Params(0.0, 2e-3, 1), Bounds()), "rate");
  CHECK_THROWS(SamplePoissonArrivals(Params(-5.0, 2e-3, 1), Bounds()), "rate");
  CHECK_THROWS(SamplePoissonArrivals(Params(std::nan(""), 2e-3, 1), Bounds()), "rate");
  CHECK_THROWS(SamplePoissonArrivals(Params(1000.0, 0.0, 1), Bounds()), "exposure");
  CHECK_THROWS(SamplePoissonArrivals(Params(1000.0, 5e-3, 1), Bounds()), "endpoint");   // 5 ms > 3 ms endpoint
  CHECK_THROWS(SamplePoissonArrivals(Params(1000.0, 3e-3, 1), Bounds()), "endpoint");   // endpoint == exposure
  PoissonParameters negativeRegion = Params(1000.0, 2e-3, 1);
  negativeRegion.regionHalfSizeNm = {-1, 1, 1};
  CHECK_THROWS(SamplePoissonArrivals(negativeRegion, Bounds()), "half size");
  CHECK_THROWS(SamplePoissonArrivals(Params(1000.0, 2e-3, 1), Bounds(0)), "library is empty");
}

void TestArrivalRates()
{
  // The 4 um box: m = (4e-6 m)^3 * 1000 kg/m^3 = 6.4e-14 kg; 1 Gy = 6.4e-14 J.
  const double e = 1.602176634e-19;
  const auto r = ComputeArrivalRates(1e4, 1.0, 2000.0, {100, 200, 300}, 1, 42);
  CHECK(std::abs(r.boxMassKg - 6.4e-14) < 1e-26);
  CHECK(std::abs(r.targetEnergyEv - 6.4e-14 / e) < 1e-6);
  CHECK(r.targetEnergyEv > 3.99e5 && r.targetEnergyEv < 4.0e5);
  CHECK(r.doseRateGyPerS == 1e4 && r.targetDoseGy == 1 && r.boxHalfSideNm == 2000);
  CHECK(r.packetCount == 3 && r.emptyCount == 1 && r.launchedCount == 4);
  CHECK(r.hitFraction == 0.75);
  CHECK(r.packetDepositSumEv == 600 && r.emptyDepositSumEv == 42);
  // Thinning: the empty crossing counts as 0 eV in ebar_launched; its 42 eV is reported only.
  CHECK(r.meanDepositPerLaunchedEv == 150);
  CHECK(r.meanDepositPerPacketEv == 200);
  const double launched = 1e4 * r.boxMassKg / (150 * e);
  CHECK(std::abs(r.launchedRateHz - launched) <= 1e-12 * launched);
  CHECK(std::abs(r.packetRateHz - 0.75 * launched) <= 1e-12 * launched);
  // packet rate = Ddot * m / ebar_packet.
  CHECK(std::abs(r.packetRateHz - 1e4 * r.boxMassKg / (200 * e)) <= 1e-12 * launched);
  // Without empties both rates agree.
  const auto full = ComputeArrivalRates(1e4, 1.0, 2000.0, {100, 200, 300}, 0, 0);
  CHECK(full.hitFraction == 1 && full.launchedRateHz == full.packetRateHz);

  CHECK_THROWS(ComputeArrivalRates(0, 1, 2000, {1}, 0, 0), "dose rate");
  CHECK_THROWS(ComputeArrivalRates(1e4, 0, 2000, {1}, 0, 0), "the dose must");
  CHECK_THROWS(ComputeArrivalRates(1e4, 1, 0, {1}, 0, 0), "half side");
  CHECK_THROWS(ComputeArrivalRates(1e4, 1, 2000, {}, 0, 0), "library is empty");
  CHECK_THROWS(ComputeArrivalRates(1e4, 1, 2000, {1, -1}, 0, 0), "negative");
  CHECK_THROWS(ComputeArrivalRates(1e4, 1, 2000, {0, 0}, 0, 0), "deposit no energy");
  CHECK_THROWS(ComputeArrivalRates(1e4, 1, 2000, {1}, 1, -1), "empty crossings");
}

DoseCutoffParameters DoseParams(double target, std::uint64_t seed = 3, std::size_t first = 0, double rate = 1e6)
{
  DoseCutoffParameters p;
  p.rateHz = rate;
  p.targetEnergyEv = target;
  p.sourceSeed = seed;
  p.firstPacket = first;
  return p;
}

void TestDoseCutoffSchedule()
{
  const std::vector<double> deposits{10, 20, 30, 40, 50};
  const auto s = SampleDoseCutoffArrivals(DoseParams(55), deposits, 100);
  // E = 10, 30, 60: the third arrival reaches 55 eV, overshoot 5 eV < 30 eV.
  CHECK(s.arrivals.size() == 3);
  CHECK((s.cumulativeEnergyEv == std::vector<double>{10, 30, 60}));
  CHECK(s.arrivals[0].timeNs == 0);  // the beam starts with an arrival
  for (std::size_t k = 0; k < s.arrivals.size(); ++k)
  {
    CHECK(s.arrivals[k].sourceIndex == k && s.arrivals[k].packetIndex == k);
    CHECK(s.arrivals[k].offsetNm == (std::array<double, 3>{0, 0, 0}));
    if (k > 0) CHECK(s.arrivals[k].timeNs > s.arrivals[k - 1].timeNs);
  }
  // Reaching the target exactly stops there; just above it needs one more.
  CHECK(SampleDoseCutoffArrivals(DoseParams(60), deposits, 100).arrivals.size() == 3);
  CHECK(SampleDoseCutoffArrivals(DoseParams(60.0001), deposits, 100).arrivals.size() == 4);
  // A target below the first deposit: one arrival, at t = 0.
  const auto one = SampleDoseCutoffArrivals(DoseParams(1), deposits, 100);
  CHECK(one.arrivals.size() == 1 && one.arrivals[0].timeNs == 0);
  // firstPacket shifts the packets, never reused.
  const auto shifted = SampleDoseCutoffArrivals(DoseParams(55, 3, 2), deposits, 100);
  CHECK(shifted.arrivals.size() == 2);
  CHECK(shifted.arrivals[0].packetIndex == 2 && shifted.arrivals[1].packetIndex == 3);
  CHECK((shifted.cumulativeEnergyEv == std::vector<double>{30, 70}));
  // Same seed, same times; the times do not depend on the deposits.
  const auto again = SampleDoseCutoffArrivals(DoseParams(55), {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}, 100);
  for (std::size_t k = 0; k < s.arrivals.size(); ++k) CHECK(again.arrivals[k].timeNs == s.arrivals[k].timeNs);
  const auto other = SampleDoseCutoffArrivals(DoseParams(55, 4), deposits, 100);
  CHECK(other.arrivals[1].timeNs != s.arrivals[1].timeNs);

  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(1000), deposits, 100), "library exhausted");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55, 3, 4), deposits, 100), "library exhausted");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55), {0, 0, 0}, 100), "library exhausted");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55), deposits, 2), "censored");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(0), deposits, 100), "target energy");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55, 3, 0, 0), deposits, 100), "rate");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55), deposits, 0), "at least 1");
  CHECK_THROWS(SampleDoseCutoffArrivals(DoseParams(55), {10, -1}, 100), "negative");
}

void TestDoseCutoffStatistics()
{
  // 1 eV per packet, target 20000 eV: 20000 arrivals, 19999 exponential waits
  // of mean 1/rate = 1000 ns.
  const std::size_t n = 20000;
  const std::vector<double> deposits(n, 1.0);
  const auto s = SampleDoseCutoffArrivals(DoseParams(static_cast<double>(n), 17), deposits, n);
  CHECK(s.arrivals.size() == n);
  const double meanWait = s.arrivals.back().timeNs / static_cast<double>(n - 1);
  CHECK(std::abs(meanWait - 1000.0) < 5 * 1000.0 / std::sqrt(static_cast<double>(n - 1)));
}

void TestDoseEnergyChecks()
{
  DoseCutoffSchedule s;
  s.cumulativeEnergyEv = {10, 30, 60};
  CHECK(CheckDoseEnergy(s, 55, 60, 60).AllPassed());
  // Summed in another order: equal within 1e-12.
  CHECK(CheckDoseEnergy(s, 55, 60 * (1 + 1e-14), 60).AllPassed());
  const auto lateSource = CheckDoseEnergy(s, 55, 30, 60);
  CHECK(!lateSource.beamOffHoldsEverySource && lateSource.targetReached && !lateSource.AllPassed());
  CHECK(!CheckDoseEnergy(s, 55, 60, 59).injectedMatchesSchedule);
  const auto overshoot = CheckDoseEnergy(s, 25, 60, 60);  // already reached at 30 eV
  CHECK(!overshoot.overshootBelowOneTrack && overshoot.targetReached);
  CHECK(!CheckDoseEnergy(s, 61, 60, 60).targetReached);
  CHECK(!CheckDoseEnergy(DoseCutoffSchedule{}, 1, 0, 0).AllPassed());
  DoseCutoffSchedule single;
  single.cumulativeEnergyEv = {10};
  CHECK(CheckDoseEnergy(single, 5, 10, 10).AllPassed());
}

}  // namespace

int main()
{
  TestDeterministicParsing();
  TestDeterministicRejections();
  TestPoissonBasics();
  TestPoissonStatistics();
  TestPoissonEdgeCases();
  TestArrivalRates();
  TestDoseCutoffSchedule();
  TestDoseCutoffStatistics();
  TestDoseEnergyChecks();
  std::puts("ArrivalScheduleTest passed");
  return 0;
}
