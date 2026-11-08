#include "FiniteSchedule.hh"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
namespace FiniteSchedule {
std::vector<BeamSchedule::Arrival> Read(std::istream& input, double exposurePs, std::size_t librarySize) {
  if (!std::isfinite(exposurePs) || exposurePs <= 0 || !librarySize)
    throw std::runtime_error("invalid schedule bounds");
  std::vector<BeamSchedule::Arrival> rows;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream record(line);
    record >> std::ws;
    if (record.eof()) continue;
    double time; std::size_t index;
    if (!(record >> time >> index)) throw std::runtime_error("malformed schedule record");
    record >> std::ws;
    if (!record.eof() || !std::isfinite(time) || time < 0 || time >= exposurePs ||
        index >= librarySize || (!rows.empty() && time < rows.back().time))
      throw std::runtime_error("invalid schedule record");
    rows.push_back({time, index});
    if (rows.size() > 8) throw std::runtime_error("schedule exceeds eight-source cap");
  }
  if (input.bad()) throw std::runtime_error("schedule read failed");
  return rows;
}
}
