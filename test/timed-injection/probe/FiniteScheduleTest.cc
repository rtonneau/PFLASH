#include "FiniteSchedule.hh"
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <random>
namespace {
void Check(bool ok) { if (!ok) throw std::runtime_error("schedule assertion failed"); }
void Reject(const char* text) {
  std::istringstream in(text);
  try { FiniteSchedule::Read(in, 10, 2); } catch (const std::runtime_error&) { return; }
  throw std::runtime_error("invalid schedule accepted");
}
}
int main() {
  try {
    std::istringstream empty(""); Check(FiniteSchedule::Read(empty, 10, 2).empty());
    std::istringstream valid("0 0\n0 1\n9.999 0\n");
    auto rows=FiniteSchedule::Read(valid, 10, 2);
    Check(rows.size()==3 && rows[0].time==0 && rows[1].packet==1 && rows[2].time==9.999);
    Reject("2 0\n1 1\n"); Reject("10 0\n"); Reject("-1 0\n");
    Reject("1 2\n"); Reject("nan 0\n"); Reject("1 0 extra\n"); Reject("1\n");
    Reject("0 0\n0 0\n0 0\n0 0\n0 0\n0 0\n0 0\n0 0\n0 0\n");
    std::mt19937_64 arrivals(123), selection(456);
    auto bounded = BeamSchedule::SampleBounded(1e-20, 1, 1, arrivals, selection, 8);
    Check(bounded.empty());
    std::mt19937_64 highArrivals(123), highSelection(456);
    bool capped = false;
    try { BeamSchedule::SampleBounded(1e8, 1, 1, highArrivals, highSelection, 8); }
    catch (const std::runtime_error&) { capped = true; }
    Check(capped);
    std::mt19937_64 a1(42), a2(42), s1(73), s2(73);
    const auto ordinary = BeamSchedule::Sample(2, 1, 4, a1, s1);
    const auto limited = BeamSchedule::SampleBounded(2, 1, 4, a2, s2, 8);
    Check(ordinary.size() == limited.size());
    for (std::size_t i = 0; i < ordinary.size(); ++i)
      Check(ordinary[i].time == limited[i].time && ordinary[i].packet == limited[i].packet);
    std::cout << "FINITE_SCHEDULE_PASS\n"; return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
