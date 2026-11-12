#include "ReflectingFold.hh"

#include <cmath>
#include <stdexcept>

namespace ptirt
{

double ReflectIntoInterval(double x, double low, double high)
{
  if (!std::isfinite(x) || !std::isfinite(low) || !std::isfinite(high))
    throw std::invalid_argument("ReflectIntoInterval: non-finite argument");
  if (!(high > low)) throw std::invalid_argument("ReflectIntoInterval: high must be greater than low");
  const double length = high - low;
  double f = std::fmod(x - low, 2 * length);
  if (f < 0) f += 2 * length;
  return low + (f <= length ? f : 2 * length - f);
}

}  // namespace ptirt
