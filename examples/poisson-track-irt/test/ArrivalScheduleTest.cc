#include "ArrivalSchedule.hh"

#include <cmath>
#include <sstream>

#include "TestCheck.hh"

using namespace ptirt;

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

}  // namespace

int main()
{
  TestDeterministicParsing();
  TestDeterministicRejections();
  TestPoissonBasics();
  TestPoissonStatistics();
  TestPoissonEdgeCases();
  std::puts("ArrivalScheduleTest passed");
  return 0;
}
