#include "BoxGuard.hh"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ptirt
{

BoxGuardResult EvaluateBoxGuard(double halfBoxNm, double endpointNs, const std::string& fastestSpecies,
                                double dNm2PerNs)
{
  if (!std::isfinite(halfBoxNm) || !std::isfinite(endpointNs) || !std::isfinite(dNm2PerNs) || endpointNs < 0 ||
      dNm2PerNs < 0)
    throw std::invalid_argument("EvaluateBoxGuard: arguments must be finite, endpoint and D non-negative");
  BoxGuardResult result;
  result.halfBoxNm = halfBoxNm;
  result.endpointNs = endpointNs;
  result.fastestSpecies = fastestSpecies;
  result.fastestDiffusionNm2PerNs = dNm2PerNs;
  result.diffusionLengthNm = std::sqrt(6 * dNm2PerNs * endpointNs);
  if (halfBoxNm <= 0) return result;  // status "none", ratio 0
  result.ratio = result.diffusionLengthNm > 0 ? halfBoxNm / result.diffusionLengthNm
                                              : std::numeric_limits<double>::infinity();
  result.status = result.ratio >= kBoxGuardMinRatio ? "ok" : "warning";
  return result;
}

}  // namespace ptirt
