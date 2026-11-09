// capture-check: read a packet library back and validate it (Geant4-free).
//
//   capture-check <dir> [--expect-count <N>]
//
// Loads <dir>/library.csv and every packet with the example's compatibility
// tag (LoadPacketLibrary: format, tag, bounds and the per-packet energy
// balance deposit + escaped = primary within tolerance, index/packet
// agreement), then requires the expected packet count and at least one
// molecule per packet. Prints "CAPTURE_CHECK_OK ..." and exits 0, or prints
// "CAPTURE_CHECK_ERROR <reason>" and exits 1.
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "CaptureLibrary.hh"
#include "ExamplePhysicsTag.hh"
#include "PrecursorPacket.hh"

namespace
{

int Run(int argc, char** argv)
{
  if (argc != 2 && argc != 4)
    throw std::runtime_error("usage: capture-check <dir> [--expect-count <N>]");
  const std::filesystem::path directory = argv[1];
  long long expectedCount = -1;
  if (argc == 4)
  {
    if (std::string(argv[2]) != "--expect-count")
      throw std::runtime_error(std::string("unknown option ") + argv[2]);
    std::size_t used = 0;
    expectedCount = std::stoll(argv[3], &used, 10);
    if (used != std::string(argv[3]).size() || expectedCount < 1)
      throw std::runtime_error("--expect-count needs a positive integer");
  }

  const ptirt::PacketLimits limits;
  const auto library = ptirt::LoadPacketLibrary(directory, ptirt::kPacketCompatibilityTag, limits);
  if (expectedCount > 0 && library.packets.size() != static_cast<std::size_t>(expectedCount))
    throw std::runtime_error("expected " + std::to_string(expectedCount) + " packets, found " +
                             std::to_string(library.packets.size()));

  for (std::size_t i = 0; i < library.packets.size(); ++i)
  {
    const auto& packet = library.packets[i];
    const auto& entry = library.entries[i];
    if (packet.molecules.empty()) throw std::runtime_error(entry.file + " holds no molecule");
    // Restate the energy ledger explicitly (Read already enforces it).
    const double residual = packet.energyDepositEv + packet.escapedEnergyEv - packet.primaryEnergyEv;
    const double bound = limits.energyAbsTolEv + limits.energyRelTol * packet.primaryEnergyEv;
    if (!(std::abs(residual) <= bound))
      throw std::runtime_error(entry.file + ": energy ledger does not balance");
    std::cout << "packet " << entry.packetIndex << " " << entry.file << " seed=" << entry.physicsSeed
              << " molecules=" << packet.molecules.size() << " primaryEv=" << packet.primaryEnergyEv
              << " depositEv=" << packet.energyDepositEv << " escapedEv=" << packet.escapedEnergyEv
              << " residualEv=" << residual << " boundEv=" << bound << '\n';
  }

  const auto totals = ptirt::SumLibrary(library.entries);
  std::ostringstream ok;
  ok.precision(17);
  ok << "CAPTURE_CHECK_OK packets=" << totals.packetCount << " molecules=" << totals.moleculeCount
     << " maxAbsResidualEv=" << totals.maxAbsResidualEv;
  std::cout << ok.str() << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  try
  {
    return Run(argc, argv);
  }
  catch (const std::exception& error)
  {
    std::cerr << "CAPTURE_CHECK_ERROR " << error.what() << std::endl;
    return 1;
  }
}
