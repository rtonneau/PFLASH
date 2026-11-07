#include "TrackPacket.hh"
#include "BeamSchedule.hh"
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
void Check(bool b)
{
  if (!b)
    throw std::runtime_error("test failed");
}
template <class F> void Reject(F f)
{
  bool rejected = false;
  try
  {
    f();
  }
  catch (const std::exception &)
  {
    rejected = true;
  }
  Check(rejected);
}
int main()
{
  try
  {
    Packets::Packet p;
    p.compatibility = "fixture";
    p.massKg = 1.25e-7;
    p.halfSide = .25;
    p.entry = {.1, -.2, -.25};
    Packets::Molecule m;
    m.definition = "H2O";
    m.occupancy = {2, 2, 2, 2, 1, 0};
    m.status = 1;
    m.direction = {0, 0, 1};
    m.globalTime = .001;
    m.position = {1e-16, -.12345678901234567, .25};
    p.molecules.push_back(m);
    auto roundtrip = [&](const Packets::Packet &v)
    {
      std::stringstream s;
      Packets::Write(s, v);
      return Packets::Read(s, "fixture");
    };
    Check(roundtrip(p) == p);
    p.molecules.clear();
    Check(roundtrip(p) == p);
    Reject(
        [&]
        {
          auto q = p;
          q.energyDeposit = std::numeric_limits<double>::quiet_NaN();
          roundtrip(q);
        });
    Reject(
        [&]
        {
          auto q = p;
          q.massKg = 0;
          roundtrip(q);
        });
    Reject(
        [&]
        {
          auto q = p;
          q.entry[0] = 1;
          roundtrip(q);
        });
    Reject(
        [&]
        {
          auto q = p;
          q.molecules = {m};
          q.molecules[0].status = 2;
          roundtrip(q);
        });
    std::stringstream s;
    Packets::Write(s, p);
    auto text = s.str();
    Reject(
        [&]
        {
          std::stringstream in(text);
          Packets::Read(in, "other");
        });
    Reject(
        [&]
        {
          auto bad = text;
          bad.replace(bad.find(" 1 "), 3, " 9 ");
          std::stringstream in(bad);
          Packets::Read(in, "fixture");
        });
    Reject(
        [&]
        {
          std::stringstream in(text.substr(0, text.size() / 2));
          Packets::Read(in, "fixture");
        });
    Reject(
        [&]
        {
          std::stringstream in(text + "junk");
          Packets::Read(in, "fixture");
        });
    Reject([] { BeamSchedule::Rate(1, 1, 0); });
    Reject([] { BeamSchedule::Dose(-1, 1); });
    Check(std::abs(BeamSchedule::Rate(10, 2, 4) * BeamSchedule::Dose(4, 2) - 10) < 1e-12);
    // Predeclared 6-sigma bounds, 100000 fixed-window histories, expected count 8.
    std::mt19937_64 arrivals(1729), selection(2917);
    double sum = 0, sum2 = 0, waitSum = 0;
    int waits = 0;
    for (int i = 0; i < 100000; ++i)
    {
      auto a = BeamSchedule::Sample(8, 1, 4, arrivals, selection);
      double n = double(a.size());
      sum += n;
      sum2 += n * n;
      for (auto x : a)
        Check(x.time >= 0 && x.time < 1 && x.packet < 4);
    }
    double mean = sum / 100000, variance = sum2 / 100000 - mean * mean;
    Check(std::abs(mean - 8) < 6 * std::sqrt(8. / 100000));
    Check(std::abs(variance - 8) < 6 * std::sqrt((8. + 128.) / 100000));
    std::exponential_distribution<double> exponential(8);
    for (int i = 0; i < 100000; ++i)
      waitSum += exponential(arrivals);
    Check(std::abs(waitSum / 100000 - .125) < 6 * .125 / std::sqrt(100000.));
    std::mt19937_64 a1(42), a2(42), s1(1), s2(99);
    auto x = BeamSchedule::Sample(8, 1, 4, a1, s1), y = BeamSchedule::Sample(8, 1, 4, a2, s2);
    Check(x.size() == y.size());
    for (std::size_t i = 0; i < x.size(); ++i)
      Check(x[i].time == y[i].time);
    // Compound Poisson dose: whole packet energy, including the zero-deposit packet.
    const double energy[4] = {0, .1, .2, .5};
    double deposited = 0, deposited2 = 0;
    std::mt19937_64 doseArrivals(839), doseSelection(853);
    for (int i = 0; i < 100000; ++i)
    {
      double e = 0;
      for (auto a : BeamSchedule::Sample(8, 1, 4, doseArrivals, doseSelection))
        e += energy[a.packet];
      deposited += e;
      deposited2 += e * e;
    }
    const double meanEnergy = deposited / 100000;
    Check(std::abs(meanEnergy - 1.6) < 6 * std::sqrt(.6 / 100000));
    Check(std::abs(deposited2 / 100000 - meanEnergy * meanEnergy - .6) < .03);
    Check(BeamSchedule::Dose(0, 1) == 0);

    Reject(
        [&]
        {
          auto q = p;
          q.molecules = {m};
          q.molecules[0].globalTime = 2;
          roundtrip(q);
        });
    Reject(
        [&]
        {
          auto q = p;
          q.energyDeposit = .6;
          roundtrip(q);
        });
    std::cout << "PACKET_AND_SOURCE_PASS mean=" << mean << " variance=" << variance
              << " meanWait=" << waitSum / 100000 << '\n';
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
