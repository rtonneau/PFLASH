#include "BeamSchedule.hh"
#include "TrackPacket.hh"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
void Check(bool b, const char *msg)
{
  if (!b)
    throw std::runtime_error(msg);
}
std::map<std::string, long long> Counts(const std::string &path)
{
  std::ifstream in(path);
  double time;
  Check(bool(in >> time) && time == .002, "chemistry horizon mismatch");
  std::map<std::string, long long> c;
  std::string name;
  long long n;
  while (in >> name >> n)
    c[name] = n;
  Check(in.eof(), "invalid counts");
  return c;
}
int main(int argc, char **argv)
{
  try
  {
    Check(argc == 2, "directory required");
    std::string dir = argv[1];
    double sums[2]{}, squares[2]{}, mass = 0;
    std::map<std::string, long long> direct, imported;
    std::ofstream report(dir + "/summary.txt");
    report << std::setprecision(17);
    for (int group = 0; group < 2; ++group)
    {
      double minX = 1, maxX = -1, minY = 1, maxY = -1;
      int quadrants[4]{};
      for (int i = 0; i < 8; ++i)
      {
        std::string base = dir + "/" + (group ? "holdout" : "library") + std::to_string(i);
        std::ifstream in(base + ".packet");
        std::string magic, identity;
        int version;
        in >> magic >> version >> std::quoted(identity);
        in.clear();
        in.seekg(0);
        auto p = Packets::Read(in, identity);
        mass = p.massKg;
        Check(p.energyDeposit <= .500000001 && p.secondaryDeposit <= p.energyDeposit,
              "energy accounting invalid");
        sums[group] += p.energyDeposit;
        squares[group] += p.energyDeposit * p.energyDeposit;
        minX = std::min(minX, p.entry[0]);
        maxX = std::max(maxX, p.entry[0]);
        minY = std::min(minY, p.entry[1]);
        maxY = std::max(maxY, p.entry[1]);
        ++quadrants[(p.entry[0] >= 0 ? 1 : 0) + (p.entry[1] >= 0 ? 2 : 0)];
        report << (group ? "holdout" : "library") << i << " Edep_MeV " << p.energyDeposit
               << " secondary_MeV " << p.secondaryDeposit << " entry_mm " << p.entry[0] << ' '
               << p.entry[1] << " precursors " << p.molecules.size() << '\n';
        if (!group)
        {
          std::ifstream rt(base + ".roundtrip");
          Check(Packets::Read(rt, identity) == p, "lossless roundtrip failed");
          for (auto [k, n] : Counts(base + ".direct"))
            direct[k] += n;
          for (auto [k, n] : Counts(base + ".chemistry"))
            imported[k] += n;
        }
      }
      report << "coverage " << group << ' ' << minX << ' ' << maxX << ' ' << minY << ' ' << maxY
             << " quadrants";
      for (int n : quadrants)
        report << ' ' << n;
      report << '\n';
    }
    double means[2], se[2];
    for (int g = 0; g < 2; ++g)
    {
      means[g] = sums[g] / 8;
      se[g] = std::sqrt((squares[g] - sums[g] * sums[g] / 8) / 7 / 8);
      report << "sample " << g << " mean_MeV " << means[g] << " SE_MeV " << se[g] << '\n';
    }
    double z = (means[0] - means[1]) / std::hypot(se[0], se[1]);
    report << "holdout_z " << z << " massKg " << mass << " rate_per_Gy_s "
           << BeamSchedule::Rate(1, mass, means[0]) << '\n';
    Check(std::abs(z) < 6, "holdout discrepancy");
    for (auto [name, n] : imported)
      direct.try_emplace(name, 0);
    for (auto [name, n] : direct)
    {
      auto m = imported[name];
      double bound = 6 * std::sqrt(double(n + m)) + .02 * (n + m);
      report << "chemistry " << name << " direct " << n << " import " << m << " difference "
             << m - n << " bound " << bound << '\n';
      Check(std::abs(double(n - m)) <= bound, "chemistry discrepancy");
    }
    auto first = Counts(dir + "/library0.direct"), repeat = Counts(dir + "/repeat.chemistry");
    for (auto [name, n] : repeat)
      first.try_emplace(name, 0);
    for (auto [name, n] : first)
    {
      auto m = repeat[name];
      report << "repeat " << name << " first " << n << " second " << m << '\n';
      Check(std::abs(double(n - m)) <= 6 * std::sqrt(double(n + m)) + .02 * (n + m),
            "repeat discrepancy");
    }
    report << "VALIDATED\n";
    std::cout << "LIBRARY_HOLDOUT_AND_CHEMISTRY_PASS\n";
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
