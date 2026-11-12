#include <cmath>
#include <limits>

#include "ReflectingFold.hh"
#include "TestCheck.hh"

using ptirt::ReflectIntoInterval;

namespace
{

void Near(double got, double want)
{
  CHECK(std::fabs(got - want) < 1e-12);
}

void TestUnitInterval()
{
  Near(ReflectIntoInterval(2.1, 0, 1), 0.1);
  Near(ReflectIntoInterval(2.9, 0, 1), 0.9);
  Near(ReflectIntoInterval(1.0, 0, 1), 1.0);
  Near(ReflectIntoInterval(-0.3, 0, 1), 0.3);
  Near(ReflectIntoInterval(1.3, 0, 1), 0.7);
  Near(ReflectIntoInterval(-2.5, 0, 1), 0.5);
  Near(ReflectIntoInterval(-1.0, 0, 1), 1.0);
  Near(ReflectIntoInterval(0.4, 0, 1), 0.4);
  Near(ReflectIntoInterval(0.0, 0, 1), 0.0);
}

void TestShiftedInterval()
{
  Near(ReflectIntoInterval(-2.5, -5, -3), -3.5);
  Near(ReflectIntoInterval(-5.5, -5, -3), -4.5);
}

void TestRefusals()
{
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS(ReflectIntoInterval(nan, 0, 1), "non-finite");
  CHECK_THROWS(ReflectIntoInterval(inf, 0, 1), "non-finite");
  CHECK_THROWS(ReflectIntoInterval(0.5, -inf, 1), "non-finite");
  CHECK_THROWS(ReflectIntoInterval(0.5, 1, 1), "greater");
  CHECK_THROWS(ReflectIntoInterval(0.5, 2, 1), "greater");
}

}  // namespace

int main()
{
  TestUnitInterval();
  TestShiftedInterval();
  TestRefusals();
  std::puts("ReflectingFoldTest passed");
  return 0;
}
