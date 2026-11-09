#include "TrackPacket.hh"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv)
{
  try
  {
    if (argc != 3)
      throw std::runtime_error("packet and output prefix required");
    std::ifstream in(argv[1]);
    std::string magic, identity;
    int version;
    in >> magic >> version >> std::quoted(identity);
    in.clear();
    in.seekg(0);
    auto original = Packets::Read(in, identity);
    for (int i = 0; i < 4; ++i)
    {
      auto p = original;
      if (i == 0)
        p.compatibility = "incompatible";
      if (i == 1)
        p.halfSide *= 2;
      if (i == 2)
      {
        if (p.molecules.empty())
          throw std::runtime_error("nonempty fixture needed");
        p.molecules[0].definition = "UNKNOWN";
      }
      if (i == 3)
      {
        p.molecules[0].occupancy = {16, 16, 16, 16, 16, 16};
      }
      std::ofstream out(std::string(argv[2]) + std::to_string(i) + ".packet");
      Packets::Write(out, p);
    }
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
