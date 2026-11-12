// ReflectingFold: the example's own reflection of a coordinate into an interval.
// Geant4-free.
//
// Fold of the real line onto [low, high] with mirror images at both ends, for any
// overshoot (period 2 * (high - low)). A point exactly on an end stays there.
#pragma once

namespace ptirt
{

// Throws std::invalid_argument when x, low or high is not finite, or high <= low.
double ReflectIntoInterval(double x, double low, double high);

}  // namespace ptirt
