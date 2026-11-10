// BoxGuard: is the reflecting chemistry box large compared with how far the
// fastest species diffuses in the history? Geant4-free.
//
// The diffusion length is sqrt(6 D t) for the fastest species and the endpoint.
// With a box half side below kBoxGuardMinRatio times that length, reflections
// are frequent and the "unbounded" histories no longer compare with the boxed one.
#pragma once

#include <string>

namespace ptirt
{

inline constexpr double kBoxGuardMinRatio = 3.0;

struct BoxGuardResult
{
  std::string status = "none";  // "none" (no box), "ok" or "warning"
  double ratio = 0;             // halfBoxNm / diffusionLengthNm; 0 when status is "none"
  double diffusionLengthNm = 0;
  double halfBoxNm = 0;
  std::string fastestSpecies;
  double fastestDiffusionNm2PerNs = 0;
  double endpointNs = 0;
};

// halfBoxNm <= 0 gives status "none". dNm2PerNs and endpointNs must be >= 0 and
// finite, else std::invalid_argument. A zero diffusion length (nothing diffuses)
// gives status "ok".
BoxGuardResult EvaluateBoxGuard(double halfBoxNm, double endpointNs, const std::string& fastestSpecies,
                                double dNm2PerNs);

}  // namespace ptirt
