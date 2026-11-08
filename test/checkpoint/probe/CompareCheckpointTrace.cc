#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// A bimolecular callback's A/B argument order is not a physical observable.
// Keep the event order, time, stable track identity and species, but compare
// each unordered reactant pair canonically. Positions and RNG remain exact.
std::string Canonical(const std::string &line)
{
  if (!line.starts_with("reaction ")) return line;
  std::istringstream input(line);
  std::string kind, time, speciesA, speciesB;
  int idA = 0, idB = 0;
  if (!(input >> kind >> time >> idA >> speciesA >> idB >> speciesB))
    throw std::runtime_error("malformed reaction row");
  if (idB < idA) {
    std::swap(idA, idB);
    std::swap(speciesA, speciesB);
  }
  return kind + " " + time + " " + std::to_string(idA) + " " + speciesA
       + " " + std::to_string(idB) + " " + speciesB;
}

int main(int argc, char **argv)
{
  try {
    if (argc != 5) throw std::runtime_error("usage: compare oracle|capture left right eventCount");
    const int events = std::stoi(argv[4]);
    if (events < 1) throw std::runtime_error("empty event set");
    bool valid = true;
    for (int id = 0; id < events; ++id) {
      const auto stem = "event-" + std::to_string(id);
      if (std::string(argv[1]) == "oracle") {
        std::ifstream a(std::filesystem::path(argv[2]) / (stem + ".trace"));
        std::ifstream b(std::filesystem::path(argv[3]) / (stem + ".trace"));
        if (!a || !b) throw std::runtime_error("missing trace " + stem);
        std::string left, right;
        std::size_t line = 0;
        bool complete = false, mesh = false, reaction = false;
        while (true) {
          const bool hasLeft = static_cast<bool>(std::getline(a, left));
          const bool hasRight = static_cast<bool>(std::getline(b, right));
          if (!hasLeft && !hasRight) break;
          ++line;
          if (hasLeft != hasRight || Canonical(left) != Canonical(right)) {
            std::cerr << stem << " first mismatch line " << line << "\nA: " << left << "\nB: " << right << '\n';
            valid = false;
            break;
          }
          complete |= left.starts_with("chemistry-end ");
          mesh |= left.starts_with("mesh ");
          reaction |= left.starts_with("reaction ");
        }
        if (!complete || !mesh || !reaction) {
          std::cerr << stem << " lacks matched complete reaction/mesh trace\n";
          valid = false;
        }
        std::cout << stem << " compared " << line << " lines\n";
      } else if (std::string(argv[1]) == "capture") {
        std::ifstream capture(std::filesystem::path(argv[2]) / (stem + ".capture"));
        if (!capture) throw std::runtime_error("missing capture " + stem);
        double requested = 0, actual = 0;
        int exact = 0, pendingKnown = 0, restorable = 0;
        std::size_t pending = 0;
        std::string key;
        capture >> key >> requested >> key >> actual >> key >> exact
                >> key >> pendingKnown >> key >> pending >> key >> restorable;
        if (!capture) throw std::runtime_error("invalid capture " + stem);
        std::cout.precision(17);
        std::cout << stem << " requested=" << requested << " actual=" << actual
                  << " exact=" << exact << " pendingKnown=" << pendingKnown
                  << " pending=" << pending << " restorable=" << restorable << '\n';
        if (!exact || !pendingKnown || !pending || !restorable) valid = false;
      } else throw std::runtime_error("unknown comparison mode");
    }
    if (!valid) {
      std::cerr << "NO-GO: required exactness evidence is absent or unequal; fresh restart is not qualified.\n";
      return 1;
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
