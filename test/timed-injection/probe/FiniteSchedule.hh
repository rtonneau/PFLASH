#pragma once
#include "BeamSchedule.hh"
#include <istream>
#include <vector>
namespace FiniteSchedule {
std::vector<BeamSchedule::Arrival> Read(std::istream&, double exposurePs, std::size_t librarySize);
}
