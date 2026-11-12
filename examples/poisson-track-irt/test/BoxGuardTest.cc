#include <cmath>
#include <limits>

#include "BoxGuard.hh"
#include "TestCheck.hh"

using namespace ptirt;

namespace
{

void TestLength()
{
  const auto r = EvaluateBoxGuard(20000, 1e6, "e_aq", 4.9);
  const double want = std::sqrt(6 * 4.9 * 1e6);
  CHECK(std::fabs(r.diffusionLengthNm - want) / want < 1e-12);
  CHECK(r.diffusionLengthNm > 5421 && r.diffusionLengthNm < 5423);
  CHECK(r.fastestSpecies == "e_aq");
  CHECK(r.fastestDiffusionNm2PerNs == 4.9);
  CHECK(r.endpointNs == 1e6);
  CHECK(r.halfBoxNm == 20000);
}

void TestStatus()
{
  CHECK(EvaluateBoxGuard(0, 1e6, "e_aq", 4.9).status == "none");
  CHECK(EvaluateBoxGuard(-1, 1e6, "e_aq", 4.9).status == "none");
  const double length = std::sqrt(6 * 4.9 * 1e6);
  CHECK(EvaluateBoxGuard(3.5 * length, 1e6, "e_aq", 4.9).status == "ok");
  CHECK(EvaluateBoxGuard(2.5 * length, 1e6, "e_aq", 4.9).status == "warning");
  const auto edge = EvaluateBoxGuard(kBoxGuardMinRatio * length, 1e6, "e_aq", 4.9);
  CHECK(edge.status == "ok" && std::fabs(edge.ratio - 3.0) < 1e-12);
  // Nothing diffuses: no warning.
  CHECK(EvaluateBoxGuard(100, 1e6, "", 0).status == "ok");
}

void TestRefusals()
{
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS(EvaluateBoxGuard(nan, 1, "a", 1), "finite");
  CHECK_THROWS(EvaluateBoxGuard(1, -1, "a", 1), "non-negative");
  CHECK_THROWS(EvaluateBoxGuard(1, 1, "a", -1), "non-negative");
}

}  // namespace

int main()
{
  TestLength();
  TestStatus();
  TestRefusals();
  std::puts("BoxGuardTest passed");
  return 0;
}
