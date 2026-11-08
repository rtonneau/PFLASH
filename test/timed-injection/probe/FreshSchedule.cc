#include "BeamSchedule.hh"
#include <iomanip>
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
int main(int argc, char** argv) {
  try {
    if (argc != 5) throw std::runtime_error("mean_MeV doseRate_Gy_s arrival_seed schedule_file");
    const double mean = std::stod(argv[1]), doseRate = std::stod(argv[2]);
    const auto seed = std::stoull(argv[3]);
    const double duration = .002 / doseRate;
    const double rate = BeamSchedule::Rate(doseRate, 6.4e-14, mean);
    std::mt19937_64 arrival(seed), unusedSelection(1);
    const auto schedule = BeamSchedule::SampleBounded(rate, duration, 1, arrival, unusedSelection, 8);
    std::ofstream out(argv[4]); out << std::setprecision(17);
    for (std::size_t i = 0; i < schedule.size(); ++i) out << schedule[i].time * 1e12 << ' ' << i << '\n';
    if (!out) throw std::runtime_error("schedule write failed");
    std::cout << std::setprecision(17) << "FRESH_SCHEDULE_PASS count " << schedule.size()
              << " lambda_s_inv " << rate << " duration_s " << duration << " seed " << seed << '\n';
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
